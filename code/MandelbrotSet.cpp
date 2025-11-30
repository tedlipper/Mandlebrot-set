#include "ComplexPlane.h"
#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <iostream>
#include <sstream>
#include <complex>
//Partnered With Anna :3 
using namespace std;

int main()
{
    unsigned int screenWidth = VideoMode::getDesktopMode().width / 2.0;
    unsigned int screenHeight = VideoMode::getDesktopMode().height / 2.0;

    VertexArray vertices(Points);

    RenderWindow window(VideoMode(screenWidth, screenHeight), "Complex plane!"); //RenderWindows is required to have string
    ComplexPlane plane(screenWidth, screenHeight);

    Font newFont;
    newFont.loadFromFile("fonts/Roboto-Black.ttf"); //put a new font
    //mine is https://fonts.google.com/specimen/Roboto

    //Will be used as  a template for all of the strings
    Text newText("", newFont, 10);
    newText.setFillColor(Color::White);
    newText.setStyle(Text::Bold);

    Event event;
    while (window.isOpen()) {
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) {
                window.close();
            }
            if (event.type == Event::MouseButtonPressed) {
                if (event.mouseButton.button == Mouse::Left) {
                    plane.zoomIn();
                    plane.setCenter({ event.mouseButton.x, event.mouseButton.y });
                    //Left click will zoomIn and call setCenter on the ComplexPlane object
                    // with the(x, y) pixel location of the mouse click
                }
                else if (event.mouseButton.button == Mouse::Right) {
                    plane.zoomOut();
                    plane.setCenter({ event.mouseButton.x, event.mouseButton.y });
                    //Right click will zoomOut and call setCenter on the ComplexPlane 
                    //object with the (x,y) pixel location of the mouse click
                }
            }
            if (event.type == Event::MouseMoved) {
                plane.setMouseLocation({ event.mouseMove.x, event.mouseMove.y });
                //Call setMouseLocation on the ComplexPlane object to store the (x,y) pixel location of the mouse click
                //This will be used later to display the mouse coordinates as it moves
            }
        }

        if (Keyboard::isKeyPressed(Keyboard::Escape)) {
            window.close();
        }
        /*
        Update Scene segment
            Call updateRender on the ComplexPlane object
            Call loadText on the ComplexPlane object
        */

        plane.updateRender();
        plane.loadText(newText);

        /*
            Draw Scene segment
            Clear the RenderWindow object
            draw the ComplexPlane object
            draw the Text object
            Display the RenderWindow object
        */

        window.clear();
        window.draw(plane);
        window.draw(newText);
        window.display();
    }

    return 0;
}
