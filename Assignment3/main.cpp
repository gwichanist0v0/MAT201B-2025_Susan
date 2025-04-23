// Karl Yerkes (modified for assignment help)
// 2022-01-20

#include "al/app/al_App.hpp"
#include "al/app/al_GUIDomain.hpp"
#include "al/math/al_Random.hpp"
#include "al/io/al_Window.hpp"

using namespace al;

#include <fstream>
#include <vector>
using namespace std;

Vec3f randomVec3f(float scale) {
  return Vec3f(rnd::uniformS(), rnd::uniformS(), rnd::uniformS()) * scale;
}

string slurp(string fileName);  // forward declaration

struct AlloApp : App {
  Parameter pointSize{"/pointSize", "", 1.0, 0.0, 2.0};
  Parameter timeStep{"/timeStep", "", 0.1, 0.01, 0.6};
  Parameter dragFactor{"/dragFactor", "", 0.1, 0.0, 0.9};
  Parameter springConstant{"/spring", "", 1.0, 0.0, 10.0};
  Parameter chargeConstant{"/charge", "", 1.0, 0.0, 10.0};
  Parameter sphereRadius{"/sphereRadius", "", 5.0, 1.0, 10.0};
  Parameter loveStrength{"/loveStrength", "", 1.0, 0.0, 10.0};
  ShaderProgram pointShader;

  Mesh mesh;  // particle positions
  vector<Vec3f> velocity;
  vector<Vec3f> force;
  vector<float> mass;
  Vec3f loveTarget;
  int mousex, mousey;
  int windHeight, windWidth;


  bool Key2Pressed = false; 
  bool Key3Pressed = false; 
  bool Key4Pressed = false;
  bool Key5Pressed = false;

  void onInit() override {
    auto GUIdomain = GUIDomain::enableGUI(defaultWindowDomain());
    auto &gui = GUIdomain->newGUI();
    gui.add(pointSize);
    gui.add(timeStep);
    gui.add(dragFactor);
    gui.add(springConstant);
    gui.add(chargeConstant);
    gui.add(sphereRadius);
    gui.add(loveStrength);
  }

  void onCreate() override {
    pointShader.compile(slurp("../point-vertex.glsl"),
                        slurp("../point-fragment.glsl"),
                        slurp("../point-geometry.glsl"));

  
    mesh.primitive(Mesh::POINTS);

    Restart();
    nav().pos(0, 0, 15);
  }

  void Restart()
  {
    
    mesh.reset();
    
    auto randomColor = []() { return HSV(rnd::uniform(), 1.0f, 1.0f); };

    for (int _ = 0; _ < 1000; _++)
    {
        Vec3f pos = randomVec3f(5);
        mesh.vertex(pos);
        mesh.color(randomColor());

        float m = 3 + rnd::normal() / 2;
        if (m < 0.5)
            m = 0.5;
        mass.push_back(m);
        mesh.texCoord(pow(m, 1.0f / 3), 0); // visual size approx radius

        velocity.push_back(randomVec3f(0.1));
        force.push_back(Vec3f(0));
    }

  }

  bool freeze = false;

  void onAnimate(double dt) override {
    if (freeze) return;

    vector<Vec3f> &position(mesh.vertices());

    if (Key2Pressed) {
     // Hooke's Law
    for (int i = 0; i < position.size(); i++) {
        Vec3f dir = position[i].normalize(sphereRadius) - position[i];
        force[i] += dir * springConstant;

    }
    Key2Pressed = false;
    }
    
    if (Key3Pressed) {
    //  Coulomb’s Law
    for (int i = 0; i < position.size(); ++i) {
      for (int j = i + 1; j < position.size(); ++j) {
        Vec3f delta = position[i] - position[j];
        float distSqr = delta.magSqr() + 0.01;  // prevent divide by 0
        Vec3f repel = delta.normalize() * (chargeConstant / distSqr);

        force[i] += repel;
        force[j] -= repel;  
      }
    }
    Key3Pressed = false;
    }

    if (Key4Pressed) {
        
        //goes to mouse position
        
        for (int i = 0; i < position.size(); i++) {
            float x = -mousex + windWidth / 2;
            float y = -mousey + windHeight / 2;
            
            loveTarget = Vec3f(-x, y, 0);

            std::cout << "x: " << x << " y: " << y << dt << std::endl;
            Vec3f toTarget = loveTarget - position[i];
            Vec3f attractionForce = toTarget.normalize() * loveStrength;
            force[i] += attractionForce;
    }

    Key4Pressed = false;
    }



    // DRAG
    for (int i = 0; i < velocity.size(); i++) {
      force[i] += -velocity[i] * dragFactor;
    }

    // SEMI-IMPLICIT EULER INTEGRATION
    for (int i = 0; i < velocity.size(); i++) {
      velocity[i] += force[i] / mass[i] * timeStep;
      position[i] += velocity[i] * timeStep;
    }

    // CLEAR FORCES
    for (auto &f : force) f.set(0);
  }

  bool onKeyDown(const Keyboard &k) override {

    if (k.key() == ' ') {
      freeze = !freeze;
    }

    if (k.key() == '1') {
      for (int i = 0; i < velocity.size(); i++) {
        force[i] += randomVec3f(1);
      }
    }

    if (k.key() == '2') {
      Key2Pressed = true;
    }

    if (k.key() == '3') {
      Key3Pressed = true;
    }

    if (k.key() == '4') {
      Key4Pressed = true;
    }

    if (k.key() == '5') {
      Restart();
    }

    return true;
  }

  bool onMouseMove(const Mouse& m) override {
    mousex = m.x();
    mousey = m.y();
    return true;
  }

  void onResize(int w, int h) override {
    windWidth = w;
    windHeight = h;
  }


  void onDraw(Graphics &g) override {
    g.clear(0.3);
    g.shader(pointShader);
    g.shader().uniform("pointSize", pointSize / 100);
    g.blending(true);
    g.blendTrans();
    g.depthTesting(true);
    g.draw(mesh);
  }
};

int main() {
  AlloApp app;
  app.configureAudio(48000, 512, 2, 0);
  app.start();
}

string slurp(string fileName) {
  fstream file(fileName);
  string returnValue = "";
  while (file.good()) {
    string line;
    getline(file, line);
    returnValue += line + "\n";
  }
  return returnValue;
}
