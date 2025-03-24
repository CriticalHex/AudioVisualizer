#include "Listener.h"
#include "Rectangle.h"

using namespace std;

int main() {
  // very cool with three monitors!
  const UINT screens = 1;
  // frequencies can be whatever you want, 1920 would be a bar per pixel
  // (smooth), but try to keep it a divisor of 1920, so there aren't any weird
  // gaps.
  const UINT frequencies = 96 * screens;
  Listener listener(frequencies);
  float volume;
  vector<float> volumes;

  sf::ContextSettings settings;
  settings.antialiasingLevel = 4;
  sf::RenderWindow window(sf::VideoMode(1920 * screens, 1080),
                          "Audio Visualizer", sf::Style::None, settings);
  window.setVerticalSyncEnabled(true);
  sf::Event event;

  sf::Vector2f winSize = sf::Vector2f(window.getSize().x, window.getSize().y);

  vector<au::Rectangle *> rects;

  float rectWidth = winSize.x / frequencies;
  for (int i = 0; i < frequencies; i++) {
    rects.push_back(new au::Rectangle(rectWidth * i, winSize.y, rectWidth,
                                      winSize.y, i, frequencies));
  }

  while (window.isOpen()) {
    while (window.pollEvent(event)) {
      if (event.type == event.Closed) {
        window.close();
      }
      if (event.type == event.KeyPressed) {
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Q)) {
          window.close();
        }
      }
    }

    volumes = listener.getFrequencyData();
    for (auto rec : rects) {
      rec->update(volumes);
    }

    window.clear();

    for (auto rec : rects) {
      rec->draw(window);
    }

    window.display();
  }

  for (int i = 0; i < rects.size(); i++) {
    delete rects[i];
  }
  rects.clear();

  return 0;
}