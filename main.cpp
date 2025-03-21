#include "Listener.h"
#include "Rectangle.h"

using namespace std;

void smooth(float *x) {
  if (*x == 0)
    *x = 1;
  *x = atan(*x + .2);
}

int main() {
  const UINT screens = 1;
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
  sf::Clock time;
  // sf::Clock framerate_clock;
  // sf::Clock frame_time_clock;
  // double frame_time = 0;
  // UINT frame_time_counter = 0;
  // UINT frames = 0;
  // UINT framerate = 0;

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
      rec->update(time.getElapsedTime().asSeconds() / 30, volumes);
    }

    window.clear();

    for (auto rec : rects) {
      rec->draw(window);
    }

    window.display();

    // frames++;

    // if (framerate_clock.getElapsedTime().asMilliseconds() >= 1000) {
    //   framerate = frames;
    //   cout << framerate << endl;
    //   frames = 0;
    //   framerate_clock.restart();
    // }

    // frame_time += frame_time_clock.getElapsedTime().asMilliseconds();
    // frame_time_clock.restart();
    // if (frame_time_counter % 100 == 0) {
    //   frame_time_counter = 0;
    //   cout << "Ms per frame: " << frame_time / 100 << endl;
    //   frame_time = 0;
    // }
    // frame_time_counter++;
  }

  for (int i = 0; i < rects.size(); i++) {
    delete rects[i];
  }
  rects.clear();

  return 0;
}