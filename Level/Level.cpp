﻿#include "Level.hpp"

#if 0
EXPORT BOOL WINAPI DllMain(
    HINSTANCE hinstDLL,
    DWORD     fdwReason,
    LPVOID    lpvReserved
) {
    return TRUE;
}
#endif

const char* info() {
    return _classes;
}

Finish::Finish(std::istream & s): color(D2D1::ColorF::White) {
    s >> left >> top >> right >> bottom >> color;
}

void Finish::load(Engine & e) {
    HRESULT hr = e.display.renderTarget->CreateSolidColorBrush(color, (ID2D1SolidColorBrush**)&b);
    CHECK;
}

void Finish::render(Engine & e) {
    auto& t = *e.display.renderTarget;
    t.FillRectangle(*this, b);
}

void Finish::Release() {
    release(b);
}

IWidget* factoryFinish(std::istream & s) {
    return new Finish(s);
}
IWidget* factoryPlayer(std::istream & s) {
    return new Player(s);
}
IWidget* factoryMagnetic(std::istream & s) {
    return new Magnetic(s);
}

Player::Player(std::istream & s) {
    s >> x >> y >> radius >> color;
}

void Player::load(Engine & e) {
    HRESULT hr = e.display.renderTarget->CreateSolidColorBrush(color, (ID2D1SolidColorBrush**)&b);
    CHECK;
    hr = e.display.renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Crimson), (ID2D1SolidColorBrush**)&a);
    CHECK;
}

void Player::render(Engine & e) {
    auto& target = *e.display.renderTarget;

    for (auto& widget : e.widgets) {
        if (widget == this) continue;
        check_collisions(e, *widget);
    }

    *this += e.physics.delta_time * speed;

    D2D1_VECTOR_2F direction = +e.input - *this;
    normalize(direction);

    for (size_t i = 0; i < photons.size(); ++i) {
        photons[i] += photons[i].speed * e.physics.delta_time;
        if (abs(nearest(e.display, photons[i]) - photons[i]) > photon_radius + photon_quant) {
            photons.erase(photons.begin() + i);
            photons.shrink_to_fit();
            continue;
        }
        else
            photons[i].render(e, *this);
    }

    bool can_throw_photon = e.physics.current_time - last_photon_time > time_between_photons;
    if (e.input.button && energy != 0 && can_throw_photon) {
        energy -= 0.1f;
        if (energy < 0) {
            energy = 0;
            return e.quit();
            // energy is out -- EIO!!!
        }
        D2D1_VECTOR_2F vec = photon_speed * direction;
        photons.emplace_back(*this + direction * (radius + photon_quant), vec);
        D2D1_VECTOR_2F k = +(speed / sqrt(1 - sqr(abs(speed) / photon_speed)) - photon_mass*vec);
        float new_speed_modulo = sqrt(sqr(abs(k)) / (1 + sqr(abs(k)/photon_speed)));
        speed = new_speed_modulo * normalize(k);
        last_photon_time = e.physics.current_time;
    }
    else {
        // friction: speed *= 0.999f;
    }

    // draw bubble
    ID2D1PathGeometry* bubble;
    ID2D1GeometrySink* sink;
    e.directFactory->CreatePathGeometry(&bubble);
    bubble->Open(&sink);

    float angle = asin(energy * 2 - 1);
    D2D1_POINT_2F rad{ x + radius * cos(angle), y - radius * sin(angle) };

    sink->BeginFigure(rad, D2D1_FIGURE_BEGIN_FILLED);
    rad.x = 2 * x - rad.x - 0.1f; // 0.1 -- to make points different when angle = 0
    sink->AddArc(D2D1::ArcSegment(rad, D2D1::SizeF(radius, radius), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE, angle > 0 ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));

    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    target.FillGeometry(bubble, a);
    release(bubble);
    release(sink);

    target.DrawEllipse(D2D1::Ellipse(*this, radius, radius), a);
    if(can_throw_photon)
        target.FillEllipse(D2D1::Ellipse(*this + radius * direction, photon_radius, photon_radius), b);
}

void Player::Release() {
    release(b);
}

void Player::check_collisions(Engine& e, const IWidget& other) {
    if (const Finish* r = dynamic_cast<const Finish*>(&other)) {
        if (abs(nearest(*r, *this) - *this) <= radius) {
            return e.quit();
        }
    }
    else if (const Magnetic* m = dynamic_cast<const Magnetic*>(&other)) {
        if (abs(nearest(*m, *this) - *this) <= radius)
            speed += Magnetic::Constant * m->induction * right_perpendicular(speed) * e.physics.delta_time;
    }
}

Magnetic::Magnetic(std::istream & s) : color(D2D1::ColorF::White) {
    s >> left >> top >> right >> bottom >> induction >> color;
}

void Magnetic::load(Engine & e) {
    HRESULT hr = e.display.renderTarget->CreateSolidColorBrush(color, (ID2D1SolidColorBrush**)&b);
    CHECK;
}

void Magnetic::render(Engine& e) {
    auto& target = e.display.renderTarget;
    D2D1_POINT_2F p { left + 10, top + 10 };

    float dx = (right - left - 20) / 10, dy = (bottom - top - 20) / 10;

    for (size_t i = 0; i < 10; ++i) {
        p.x = left + 10;
        for (size_t j = 0; j < 10; ++j) {
            target->DrawEllipse(D2D1::Ellipse(p, 10, 10), b);
            if (induction > 0) {
                target->FillEllipse(D2D1::Ellipse(p, 1, 1), b);
            }
            else {
                target->DrawLine(D2D1::Point2F(p.x - 5, p.y - 5), D2D1::Point2F(p.x + 5, p.y + 5), b);
                target->DrawLine(D2D1::Point2F(p.x - 5, p.y + 5), D2D1::Point2F(p.x + 5, p.y - 5), b);
            }
            p.x += dx;
        }
        p.y += dy;
    }
}

void Magnetic::Release() {
    release(b);
}

Player::Photon::Photon(D2D1_POINT_2F p, D2D1_VECTOR_2F s): D2D1_POINT_2F(p) {
    speed = s;
}

void Player::Photon::render(Engine& e, Player& p) {
    // draw tail
    D2D1_VECTOR_2F direction = -speed;
    normalize(direction);
    D2D1_VECTOR_2F perpendicular = right_perpendicular(direction);
    D2D1_POINT_2F point = *this;

    ID2D1PathGeometry* tail;
    e.directFactory->CreatePathGeometry(&tail);

    ID2D1GeometrySink* sink;
    tail->Open(&sink);
    sink->BeginFigure(point, D2D1_FIGURE_BEGIN_HOLLOW);

    for (float dist = 0; dist < p.photon_quant; dist += 0.1f) {
        float perp_module = 7 * sin(10 * e.physics.current_time + dist / 4);
        point = *this + dist * direction + perp_module * perpendicular;
        sink->AddLine(point);
    }

    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();

    e.display.renderTarget->DrawGeometry(tail, p.b);

    release(tail);
    release(sink);
    e.display.renderTarget->FillEllipse(D2D1::Ellipse(*this, p.photon_radius, p.photon_radius), p.b);
}
