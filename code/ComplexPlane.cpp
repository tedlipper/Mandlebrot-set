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
    // Will update the cursor when the user moves the mouse in the window screen
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
        const int width = m_pixel_size.x;                       // image width (number of pixels horizontally).
        const int height = m_pixel_size.y;                      // image height (number of pixels vertically).
        const float planeW = m_plane_size.x;                    // complex plane width (real axis span).
        const float planeH = m_plane_size.y;                    // complex plane height (imaginary axis span).
        const float centerX = m_plane_center.x - planeW * 0.5f; // starting complex x-coordinate (left edge).
        const float centerY = m_plane_center.y - planeH * 0.5f; // starting complex y-coordinate (top edge).
        const float numX = planeW / static_cast<float>(width);  // x-axis step size per pixel.
        const float numY = planeH / static_cast<float>(height); // y-axis step size per pixel.
        const int maxIterLocal = m_maxIter;                     // maximum number of iterations allowed.

        // Temporary scratch buffer that threads write into; copy into m_vArray after work completes.
        std::vector<sf::Vertex> buffer; // Declare the scratch buffer to store the results from all threads.
        buffer.resize(static_cast<size_t>(width) * static_cast<size_t>(height)); // Resize buffer to hold all pixels.

        // Determine thread count (fall back to 1 if hardware_concurrency returns 0)
		unsigned int threadCount = std::thread::hardware_concurrency(); // Get the number of open CPU threads.
        if (threadCount == 0) { threadCount = 1; } // If detection fails (returns 0), default to 1 thread.

        // Don't create more threads than pixel rows, as rows are the unit of work.
        threadCount = std::min<unsigned int>(threadCount, static_cast<unsigned int>(height));

        std::vector<std::thread> workers; // Vector to hold the worker thread objects.
        workers.reserve(threadCount); // Reserve space for the determined number of threads.

        // Define the work unit (lambda function) that each thread will execute.
        auto worker = [&](int rowStart, int rowEnd) {
            for (int i = rowStart; i < rowEnd; ++i) { // Outer loop iterates through the assigned rows.
                // Precompute y coord constant for this row
                float coordY = i * numY + centerY; // Pre-calculate imaginary part of the complex num for the row.
                for (int j = 0; j < width; ++j) { // Inner loop iterates through all columns.
                    const int idx = j + i * width; // Calculate the 1D index into the buffer array.
					// Position of vertex in screen space (pixel coordinates)
                    buffer[idx].position = { static_cast<float>(j), static_cast<float>(i) };

                    // Map pixel to complex plane coordinates (inlined for speed)
                    float coordX = j * numX + centerX; // Calculate the real part of the complex num (c_x) for this pixel.

                    // perform iteration using local maxIter (Mandelbrot calculation)
                    size_t iterations = 0; // Initialize iteration counter.
                    float x = 0.0f, y = 0.0f, x2 = 0.0f, y2 = 0.0f; // Initialize the complex number z = x + yi to 0.
                    // Loop until max iterations reached or the magnitude squared of z is greater than 4 (escape condition).
					// This bit of code is almost identical to Annas countIterations, but inlined here for performance.
                    while (iterations < static_cast<size_t>(maxIterLocal) && (x2 + y2 <= 4.0f)) {
                        y = 2.0f * x * y + coordY; // Calculate new imaginary part y_n+1 = 2*x_n*y_n + c_y.
                        x = x2 - y2 + coordX; // Calculate new real part x_n+1 = x_n^2 - y_n^2 + c_x.
                        x2 = x * x; // Update x squared.
                        y2 = y * y; // Update y squared.
                        ++iterations; // Increment the iteration count.
                    }

                    // Convert iterations to color (call existing mapper)
                    sf::Uint8 r, g, b; // Variables to store the resulting RGB components.
                    ComplexPlane::iterationsToRGB(static_cast<size_t>(iterations), r, g, b); // Convert count into RGB.
                    buffer[idx].color = sf::Color(r, g, b); // Set the color of the vertex in the buffer.
                }
            }
        };

        // Divide rows evenly among threads
        int rowsPerThread = height / static_cast<int>(threadCount); // Calculate the number of rows for each thread.
        int extra = height % static_cast<int>(threadCount); // Calculate any remainder rows to be distributed.
        int row = 0; // Initialize the starting row index for the first thread.
        for (unsigned int t = 0; t < threadCount; ++t) { // Loop to launch a worker for each thread.
            int start = row; // The starting row for the current thread.
            // Add 1 to the count for the extra to distribute the remainder.
            int count = rowsPerThread + (t < static_cast<unsigned int>(extra) ? 1 : 0); 
            int end = start + count; // The ending row (exclusive) for the current thread.
            row = end; // Update the starting row for the next thread.
            // Create and immediately start a new thread, passing the start and end rows to the lambda.
            workers.emplace_back(worker, start, end); // launch worker for [start, end)
        }

        // Join threads
        for (auto& th : workers) th.join(); // Wait here for *all* worker threads to finish their execution.

        // Copy buffer into the VertexArray on the main thread
        const size_t total = static_cast<size_t>(width) * static_cast<size_t>(height); // Calculate total number of pixels.
        for (size_t k = 0; k < total; ++k) { // Loop through the entire scratch buffer.
            m_vArray[k] = buffer[k]; // Copy the calculated vertex (position and color) from the buffer into m_vArray.
        }

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

// Original code by Anna, reworked by Ted
// size_t count is the number of iterations until escape (or the maximum).
void ComplexPlane::iterationsToRGB(size_t count, Uint8& r, Uint8& g, Uint8& b)
{
    // If inside the set, set the color to black (RGB 0, 0, 0).
    if (count >= static_cast<size_t>(m_maxIter)) // if the iteration count is more than the iteration limit ('m_maxIter').
    { r = g = b = 0; return;}// the point is (probably) inside the set (i.e., it didn't escape) so set the color to black.

    // Normalize iteration count into a double 't' in the range [0, 1).
    double t = static_cast<double>(count) / static_cast<double>(m_maxIter);

	double cycles = 5.0; // Number of times the hue spectrum will repeat over the range of iterations.
    double hue = fmod(cycles * t, 1.0) * 360.0;// Calculate the Hue value (in range, 0 to 360):
    // 1. (cycles * t): Scales the normalized value 't' across multiple color cycles.
    // 2. fmod(..., 1.0): Takes the fractional part, wrapping the value back into [0, 1).
    // 3. * 360.0: Converts the [0, 1) range to [0, 360) which is the range of the HSV hue field.
    double sat = 0.85; // Set the Saturation (color purity) to 85% so it's easier on the eyes.
    double val = 1.0;  // Set the Value (brightness/lightness) to 100%.

    // HSV -> RGB conversion
    // The following lines implement the standard HSV to RGB conversion formulas.
    double C = val * sat;   // C (Chroma): The maximum amount of colored light
    double X = C * (1.0 - fabs(fmod(hue / 60.0, 2.0) - 1.0)); 
    // X (Intermediate value): Used to calculate the second largest RGB component.
    // The expression inside fabs() calculates where 'hue' is within a 60-degree sextant.
    double m = val - C; 
    // m (Match): The grayscale offset(how much black is added to all three components to achieve the correct brightness)
    double rf, gf, bf;
    // Temporary variables to hold the floating-point Red, Green, and Blue components (r', g', b' in [0, C] range).

    if (hue < 60) { rf = C; gf = X; bf = 0; }       // Hue [0, 60): Red is max, Green increases, Blue is zero.
    else if (hue < 120) { rf = X; gf = C; bf = 0; } // Hue [60, 120): Green is max, Red decreases, Blue is zero.
    else if (hue < 180) { rf = 0; gf = C; bf = X; } // Hue [120, 180): Green is max, Blue increases, Red is zero.
    else if (hue < 240) { rf = 0; gf = X; bf = C; } // Hue [180, 240): Blue is max, Green decreases, Red is zero.
    else if (hue < 300) { rf = X; gf = 0; bf = C; } // Hue [240, 300): Blue is max, Red increases, Green is zero.
    else { rf = C; gf = 0; bf = X; }    // Hue [300, 360]: Red is max, Blue decreases, Green is zero (wraps back to start).

    // for each Add 'm' (lightness adjustment) and scale the [0, 1] range to [0, 255], then cast to Uint8.
    r = static_cast<Uint8>((rf + m) * 255.0);   // Final Red calculation.
    g = static_cast<Uint8>((gf + m) * 255.0);   // Final Green calculation.
    b = static_cast<Uint8>((bf + m) * 255.0);   // Final Blue calculation.
}

sf::Vector2f ComplexPlane::mapPixelToCoords(sf::Vector2i mousePixel)
{
    sf::Vector2f newCoord;

    float numX = m_plane_size.x / m_pixel_size.x; // should be +c or -c
    float numY = m_plane_size.y / m_pixel_size.y; // should be +c or -c

    newCoord.x = (mousePixel.x * numX) + (m_plane_center.x - m_plane_size.x / 2.0f);
    newCoord.y = (mousePixel.y * numY) + (m_plane_center.y - m_plane_size.y / 2.0f);

    return newCoord;
}
