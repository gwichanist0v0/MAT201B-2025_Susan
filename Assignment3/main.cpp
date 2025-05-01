#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/math/al_Random.hpp"
#include "al/math/al_Vec.hpp"

using namespace al;

#include <fstream>
#include <vector>
using namespace std;

Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}

RGB randomColor() {
  float h = rnd::uniform();
  float s = 0.4f; // lower saturation for matte look
  float v = 0.9f; // bright but not shiny
  return HSV(h, s, v);
}

struct AlloApp : App {
  Parameter timeStep{"/timeStep", "", 0.1, 0.01, 0.6};
  Parameter moveSpeed{"/moveSpeed", "", 5.0, 0.1, 20.0};
  Parameter foodInterval{"/foodInterval", "", 7.0, 1.0, 30.0};
  Parameter repelStrength{"/repelStrength", "", 0.05, 0.0, 1.0};
  Parameter alignStrength{"/alignStrength", "", 0.02, 0.0, 0.5};
  Parameter cohesionStrength{"/cohesionStrength", "", 0.02, 0.0, 0.5};
  Parameter cohesionRadius{"/cohesionRadius", "", 2.0, 0.1, 10.0};
  ParameterInt totalAgents{"/totalAgents", "", 20, 5, 100};

  Light light;
  Material material;
  Mesh mesh;

  std::vector<Nav> agent;
  std::vector<float> size;
  std::vector<int> interest;

  Vec3f food;
  RGB foodColor{1, 0, 0};
  RGB leaderColor{1, 0, 0};
  double time = 0;
  bool paused = false;
  int lastAgentCount = 0;

  void generateAgents(int count) {
    agent.clear();
    size.clear();
    interest.clear();
    for (int i = 0; i < count; ++i) {
      Nav p;
      p.pos() = randomVec3f(5);
      p.quat().set(rnd::uniformS(), rnd::uniformS(), rnd::uniformS(), rnd::uniformS()).normalize();
      agent.push_back(p);
      size.push_back(rnd::uniform(0.05, 1.0));
      interest.push_back(-1);
    }
  }

  void onInit() override {
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();
    gui.add(timeStep);
    gui.add(moveSpeed);
    gui.add(foodInterval);
    gui.add(repelStrength);
    gui.add(alignStrength);
    gui.add(cohesionStrength);
    gui.add(cohesionRadius);
    gui.add(totalAgents);
  }

  void onCreate() override {
    nav().pos(0, 0, 10);
    addCone(mesh);
    mesh.scale(0.2, 0.2, 0.7);
    mesh.generateNormals();
    light.pos(0, 10, 10);
    generateAgents(totalAgents);
    lastAgentCount = totalAgents;
  }

  void onAnimate(double dt) override {
    if (totalAgents != lastAgentCount) {
      generateAgents(totalAgents);
      lastAgentCount = totalAgents;
    }

    if (paused) return;
    if (time > foodInterval) {
      time -= foodInterval;
      food = randomVec3f(15);
      foodColor = randomColor();
      leaderColor = foodColor;  // match leader to food color
    }
    time += dt;

    for (int i = 0; i < agent.size(); ++i) {
      if (interest[i] >= 0) continue;
      for (int j = i + 1; j < agent.size(); ++j) {
        float difference = size[j] - size[i];
        if (difference > 0 && difference < 0.1) {
          interest[i] = j;
        }
      }
    }

    agent[0].faceToward(food, 0.1);
    agent[0].moveF(moveSpeed);
    agent[0].step(dt);

    for (int i = 1; i < agent.size(); ++i) {
      if (interest[i] >= 0) {
        agent[i].faceToward(agent[interest[i]].pos(), 0.1);
        agent[i].nudgeToward(agent[interest[i]].pos(), -0.1);
      } else {
        agent[i].faceToward(food, 0.1);
      }
    }

    for (int i = 1; i < agent.size(); ++i) {
      Vec3f repelForce;
      for (int j = 0; j < agent.size(); ++j) {
        if (i == j || j == 0) continue;
        Vec3f diff = agent[i].pos() - agent[j].pos();
        float dist = diff.mag();
        if (dist > 0.01 && dist < 1.0) {
          float force = repelStrength / (dist * dist);
          repelForce += diff.normalize() * force;
        }
      }
      agent[i].nudge(repelForce);
    }

    for (int i = 1; i < agent.size(); ++i) {
      Vec3f avgHeading;
      int count = 0;
      for (int j = 0; j < agent.size(); ++j) {
        if (i == j || j == 0) continue;
        Vec3f diff = agent[j].pos() - agent[i].pos();
        float dist = diff.mag();
        if (dist < 1.0) {
          avgHeading += agent[j].uf();
          count++;
        }
      }
      if (count > 0) {
        avgHeading /= count;
        agent[i].nudge(avgHeading.normalize() * alignStrength);
      }
    }

    for (int i = 1; i < agent.size(); ++i) {
      Vec3f centerOfMass;
      int count = 0;
      for (int j = 1; j < agent.size(); ++j) {
        if (i == j) continue;
        Vec3f diff = agent[j].pos() - agent[i].pos();
        float dist = diff.mag();
        if (dist < cohesionRadius) {
          centerOfMass += agent[j].pos();
          count++;
        }
      }
      if (count > 0) {
        centerOfMass /= count;
        Vec3f cohesionForce = centerOfMass - agent[i].pos();
        cohesionForce.normalize();
        agent[i].nudge(cohesionForce * cohesionStrength.get());
      }
    }

    for (int i = 0; i < agent.size(); ++i) {
      agent[i].moveF(moveSpeed);
      agent[i].step(dt);
    }
  }

  bool onKeyDown(const Keyboard &k) override {
    if (k.key() == ' ') {
      paused = !paused;
    }
  }

  void onDraw(Graphics &g) override {
    g.clear(0.27);
    g.depthTesting(true);
    g.lighting(true);
    light.globalAmbient(RGB(0.1));
    light.ambient(RGB(0));
    light.diffuse(RGB(0.8, 0.8, 0.8));  // softer neutral light
    g.light(light);
    material.specular(RGB(0.1));  // dimmer specular for matte feel
    material.shininess(50);
    g.material(material);

    for (int i = 0; i < agent.size(); ++i) {
      g.pushMatrix();
      g.translate(agent[i].pos());
      g.rotate(agent[i].quat());
      g.scale(size[i]);
      if (i == 0) g.color(leaderColor);
      else g.color(1.0, 1.0, 1.0, 0.5);  // white with half transparency
      g.draw(mesh);
      g.popMatrix();
    }

    Mesh m;
    addSphere(m, 0.1);
    m.generateNormals();
    g.pushMatrix();
    g.translate(food);
    g.color(foodColor);
    g.draw(m);
    g.popMatrix();
  }
};

int main() {
  AlloApp app;
  app.configureAudio(48000, 512, 2, 0);
  app.start();
}
