#include "ComplexPlane.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <iostream>
#include <sstream>
#include <complex>
//Partnered with Anna :3 
using namespace std;

ComplexPlane::ComplexPlane(int pixelWidth, int pixelHeight)
{
    m_pixel_size = { pixelWidth, pixelHeight };
    m_aspectRatio = static_cast<float>(pixelHeight) / static_cast<float>(pixelWidth);
    m_plane_center = { 0,0 };
    m_plane_size = { BASE_WIDTH,BASE_HEIGHT * m_aspectRatio };
    m_zoomCount = 0;
    m_state = State::CALCULATING;
    m_mouseLocation = { 0.f, 0.f };
    //Initialize VertexArray with Points and pixelWidth with pixelHeight
    m_vArray.setPrimitiveType(Points);
    m_vArray.resize(pixelWidth * pixelHeight);
}

void ComplexPlane::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(m_vArray);
}

void ComplexPlane::zoomIn()
{
    //incremenets the zoom count to zoom in
    m_zoomCount++;

    float xNew = BASE_WIDTH * (pow(BASE_ZOOM, m_zoomCount));
    float yNew = BASE_HEIGHT * m_aspectRatio * (pow(BASE_ZOOM, m_zoomCount));

    m_plane_size = { xNew,yNew };
    m_state = State::CALCULATING;
}

void ComplexPlane::zoomOut()
{
    //decrements the zoom count for zoom out
    m_zoomCount--;

    float xNew = BASE_WIDTH * (pow(BASE_ZOOM, m_zoomCount));
    float yNew = BASE_HEIGHT * m_aspectRatio * (pow(BASE_ZOOM, m_zoomCount));

    m_plane_size = { xNew,yNew };
    m_state = State::CALCULATING;
}

void ComplexPlane::setCenter(sf::Vector2i mousePixel)
{
    m_plane_center = sf::Vector2f(ComplexPlane::mapPixelToCoords(mousePixel));
    m_state = State::CALCULATING;
}

void ComplexPlane::setMouseLocation(sf::Vector2i mousePixel)
{
    m_mouseLocation = sf::Vector2f(ComplexPlane::mapPixelToCoords(mousePixel));
}

void ComplexPlane::loadText(sf::Text& text)
{
    stringstream words;

    words.str("");

    words << "Mandelbrot Set" << endl;
    words << "Center: (" << m_plane_center.x << "," << m_plane_center.y << ")" << endl;
    //Will update the cursor when the user moves the mouse in the window screen
    words << "Cursor: (" << m_mouseLocation.x << "," << m_mouseLocation.y << ")" << endl;

    words << "Left-click to Zoom in" << endl;
    words << "Right-click to Zoom out" << endl;

    text.setString(words.str());
}

void ComplexPlane::updateRender()
{
    if (m_state == State::CALCULATING)
    {
        for (int j = 0; j < m_pixel_size.x; j++)
        {
            for (int i = 0; i < m_pixel_size.y; i++)
            {
                m_vArray[j + i * m_pixel_size.x].position = { (float)j,(float)i };

                Vector2f coords = ComplexPlane::mapPixelToCoords(Vector2i(j, i));

                int iterCount = ComplexPlane::countIterations(coords);

                sf::Uint8 r, g, b;
                ComplexPlane::iterationsToRGB(iterCount, r, g, b);

                m_vArray[j + i * m_pixel_size.x].color = { r,g,b };
            }
        }
        m_state = State::DISPLAYING;
    }
}

int ComplexPlane::countIterations(sf::Vector2f coord)
{
    size_t iterations = 0;

    float x = 0;
    float y = 0;
    float x2 = 0;
    float y2 = 0;

    while (iterations < MAX_ITER && (x2 + y2 <= 4))
    {
        y = 2 * x * y + coord.y;
        x = x2 - y2 + coord.x;
        x2 = x * x;
        y2 = y * y;

        iterations++;
    }

    return iterations;
}

//reworked by ted
void ComplexPlane::iterationsToRGB(size_t count, Uint8& r, Uint8& g, Uint8& b)
{
    if (count >= MAX_ITER) // interior of the set is always black
    {
        r = g = b = 0;
        return;
    }

    // Normalize iteration count to [0,1)
    double t = static_cast<double>(count) / static_cast<double>(MAX_ITER);

    // Parameters: number of hue cycles; saturation and value
    double cycles = 5.0;
    double hue = fmod(cycles * t, 1.0) * 360.0; // degrees
    double sat = 0.85;
    double val = 1.0;

    // HSV -> RGB conversion
    double C = val * sat;
    double X = C * (1.0 - fabs(fmod(hue / 60.0, 2.0) - 1.0));
    double m = val - C;
    double rf, gf, bf;

    if (hue < 60)        { rf = C; gf = X; bf = 0; }
    else if (hue < 120)  { rf = X; gf = C; bf = 0; }
    else if (hue < 180)  { rf = 0; gf = C; bf = X; }
    else if (hue < 240)  { rf = 0; gf = X; bf = C; }
    else if (hue < 300)  { rf = X; gf = 0; bf = C; }
    else                 { rf = C; gf = 0; bf = X; }

    r = static_cast<Uint8>((rf + m) * 255.0);
    g = static_cast<Uint8>((gf + m) * 255.0);
    b = static_cast<Uint8>((bf + m) * 255.0);
}

sf::Vector2f ComplexPlane::mapPixelToCoords(sf::Vector2i mousePixel)
{
    sf::Vector2f newCoord;

    float numX = m_plane_size.x / m_pixel_size.x; //should be +c or -c
    float numY = m_plane_size.y / m_pixel_size.y; // should be +c or -c

    newCoord.x = (mousePixel.x * numX) + (m_plane_center.x - m_plane_size.x / 2.0f);
    newCoord.y = (mousePixel.y * numY) + (m_plane_center.y - m_plane_size.y / 2.0f);

    return newCoord;
}
