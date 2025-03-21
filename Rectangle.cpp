#include "Rectangle.h"
#include <cmath>
#include <iostream>

// these functions (lerp and get color) were used to generate
// the list in Rectangle.h. Thought it was better to just have a table.
static sf::Color lerpColor(const sf::Color &color1, const sf::Color &color2,
                           float t) {
  float r = color1.r + t * (color2.r - color1.r);
  float g = color1.g + t * (color2.g - color1.g);
  float b = color1.b + t * (color2.b - color1.b);
  return sf::Color(r, g, b);
}

static sf::Color getColor(float t) {
  const float hue_range = 360.0f;
  float hue = std::fmod(t / 5.0f, 1.0f) * hue_range;
  int i = int(hue / 60) % 6;
  float f = hue / 60 - i;

  sf::Color colors[] = {sf::Color::Red,  sf::Color::Yellow, sf::Color::Green,
                        sf::Color::Cyan, sf::Color::Blue,   sf::Color::Magenta};

  sf::Color color1 = colors[i];
  sf::Color color2 = colors[(i + 1) % 6];
  return lerpColor(color1, color2, f);
}

au::Rectangle::Rectangle(int x, int y, int width, int height, int band,
                         int maxBand) {
  _rect.setPosition(sf::Vector2f(x, y));
  _rect.setFillColor(au::colors[599 - int((float(band) / maxBand) * 599)]);
  _rect.setSize(sf::Vector2f(width, height));
  _band = band;
  _maxBand = maxBand;
  _maxHeight = height;
}

au::Rectangle::~Rectangle() {}

sf::RectangleShape au::Rectangle::getRect() { return _rect; }

void au::Rectangle::update(std::vector<float> volume) {
  if (volume[_band] == 0) {
    // if volume is 0 then slowly descend, makes for a nice effect.
    _rect.setSize(sf::Vector2f(_rect.getSize().x, _rect.getSize().y * .98f));
  } else {
    // rate is used to slow the transition between the last value
    // and a new one. A value of 1 means that the transition
    // is instant, making for a reactive visualization, but sensitive
    float rate = 1.f;
    float newHeight =
        rate * (volume[_band] * _maxHeight) + (1.f - rate) * _rect.getSize().y;
    _rect.setSize(sf::Vector2f(_rect.getSize().x, newHeight));
  }
  // since rects are drawn from the top left, and we just changed the top
  // otherwise it would be an upsidedown visualization
  _rect.setOrigin(sf::Vector2f(0, _rect.getSize().y));
}

void au::Rectangle::draw(sf::RenderWindow &window) { window.draw(_rect); }