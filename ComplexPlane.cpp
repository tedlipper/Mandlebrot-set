#include "ComplexPlane.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <iostream>
#include <sstream>
#include <complex> 
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <thread>
#include <vector>
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
    m_vArray.setPrimitiveType(Points);
    m_vArray.resize(pixelWidth * pixelHeight);

    // start with the base iteration budget
    m_maxIter = static_cast<int>(MAX_ITER);
}

void ComplexPlane::draw(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(m_vArray);
}

void ComplexPlane::zoomIn()
{
    // increment zoom and shrink the plane
    ++m_zoomCount;

    float xNew = BASE_WIDTH * (pow(BASE_ZOOM, m_zoomCount));
    float yNew = BASE_HEIGHT * m_aspectRatio * (pow(BASE_ZOOM, m_zoomCount));

    m_plane_size = { xNew,yNew };

    // linear iteration growth per zoom step (prevents explosive exponential growth)
    int candidate = static_cast<int>(MAX_ITER + m_zoomCount * ITER_PER_ZOOM);
    // clamp to reasonable bounds
    m_maxIter = std::max(MIN_ITER_CAP, std::min(MAX_ITER_CAP, candidate));

    m_state = State::CALCULATING;
}

void ComplexPlane::zoomOut()
{
    // decrement zoom and expand the plane
    --m_zoomCount;

    float xNew = BASE_WIDTH * (pow(BASE_ZOOM, m_zoomCount));
    float yNew = BASE_HEIGHT * m_aspectRatio * (pow(BASE_ZOOM, m_zoomCount));

    m_plane_size = { xNew,yNew };

    // linear iteration growth per zoom step (symmetric)
    int candidate = static_cast<int>(MAX_ITER + m_zoomCount * ITER_PER_ZOOM);
    m_maxIter = std::max(MIN_ITER_CAP, std::min(MAX_ITER_CAP, candidate));

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
    words << "Iterations: " << m_maxIter << endl;

    text.setString(words.str());
}

void ComplexPlane::updateRender()
{
    if (m_state == State::CALCULATING)
    {
        // Prepare local copies to avoid repeated member access inside hot loops
        const int width = m_pixel_size.x;
        const int height = m_pixel_size.y;
        const float planeW = m_plane_size.x;
        const float planeH = m_plane_size.y;
        const float centerX = m_plane_center.x - planeW * 0.5f;
        const float centerY = m_plane_center.y - planeH * 0.5f;
        const float numX = planeW / static_cast<float>(width);
        const float numY = planeH / static_cast<float>(height);
        const int maxIterLocal = m_maxIter;

        // Scratch buffer that threads write into; copy into m_vArray after work completes.
        std::vector<sf::Vertex> buffer;
        buffer.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

        // determine thread count (fall back to 1 if hardware_concurrency returns 0)
        unsigned int threadCount = std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 1;
        // don't create more threads than rows
        threadCount = std::min<unsigned int>(threadCount, static_cast<unsigned int>(height));

        std::vector<std::thread> workers;
        workers.reserve(threadCount);

        auto worker = [&](int rowStart, int rowEnd) {
            for (int i = rowStart; i < rowEnd; ++i) {
                // precompute y coord constant for this row
                float coordY = i * numY + centerY;
                for (int j = 0; j < width; ++j) {
                    const int idx = j + i * width;
                    // position in screen space
                    buffer[idx].position = { static_cast<float>(j), static_cast<float>(i) };

                    // map pixel to complex plane coordinates (inlined for speed)
                    float coordX = j * numX + centerX;

                    // perform iteration using local maxIter
                    size_t iterations = 0;
                    float x = 0.0f, y = 0.0f, x2 = 0.0f, y2 = 0.0f;
                    while (iterations < static_cast<size_t>(maxIterLocal) && (x2 + y2 <= 4.0f)) {
                        y = 2.0f * x * y + coordY;
                        x = x2 - y2 + coordX;
                        x2 = x * x;
                        y2 = y * y;
                        ++iterations;
                    }

                    // convert iterations to color (call existing mapper)
                    sf::Uint8 r, g, b;
                    ComplexPlane::iterationsToRGB(static_cast<size_t>(iterations), r, g, b);
                    buffer[idx].color = sf::Color(r, g, b);
                }
            }
            };

        // divide rows evenly among threads
        int rowsPerThread = height / static_cast<int>(threadCount);
        int extra = height % static_cast<int>(threadCount);
        int row = 0;
        for (unsigned int t = 0; t < threadCount; ++t) {
            int start = row;
            int count = rowsPerThread + (t < static_cast<unsigned int>(extra) ? 1 : 0);
            int end = start + count;
            row = end;
            // launch worker for [start, end)
            workers.emplace_back(worker, start, end);
        }

        // join threads
        for (auto& th : workers) th.join();

        // copy buffer into the VertexArray on the main thread
        const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height);
        for (size_t k = 0; k < total; ++k) {
            m_vArray[k] = buffer[k];
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

    while (iterations < static_cast<size_t>(m_maxIter) && (x2 + y2 <= 4))
    {
        y = 2 * x * y + coord.y;
        x = x2 - y2 + coord.x;
        x2 = x * x;
        y2 = y * y;

        iterations++;
    }

    return static_cast<int>(iterations);
}

//reworked by ted
void ComplexPlane::iterationsToRGB(size_t count, Uint8& r, Uint8& g, Uint8& b)
{
    if (count >= static_cast<size_t>(m_maxIter)) // interior of the set is always black
    {
        r = g = b = 0;
        return;
    }

    // Normalize iteration count to [0,1)
    double t = static_cast<double>(count) / static_cast<double>(m_maxIter);

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

    if (hue < 60) { rf = C; gf = X; bf = 0; }
    else if (hue < 120) { rf = X; gf = C; bf = 0; }
    else if (hue < 180) { rf = 0; gf = C; bf = X; }
    else if (hue < 240) { rf = 0; gf = X; bf = C; }
    else if (hue < 300) { rf = X; gf = 0; bf = C; }
    else { rf = C; gf = 0; bf = X; }

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
