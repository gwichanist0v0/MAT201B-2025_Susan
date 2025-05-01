#include "al/app/al_App.hpp"
#include "al/graphics/al_Image.hpp" 
#include "al/graphics/al_Shapes.hpp" // addCone, addCube, addSphere
#include "al/types/al_Color.hpp"
#include "al/math/al_Interpolation.hpp"
#include "al/math/al_Random.hpp"

using namespace al;

#include <fstream> // for slurp()
#include <string> // for slurp()

Vec3f rvec() { return Vec3f(1.0, 1.0, 1.0);}
RGB rcolor() { return RGB (1.0, 1.0, 1.0); }

std::string slurp(std::string fileName); // only a declaration


class MyApp : public App{

    Mesh grid, rgb, hsv, test;
    Mesh mesh; 
    ShaderProgram shader;
    Parameter pointSize{"pointSize", 0.005, 0.005, 0.005 };

    Mesh::Vertices meshVertices;
    Mesh::Vertices gridVertices; 
    Mesh::Vertices rgbVertices;
    Mesh::Vertices hsvVertices;
    Mesh::Vertices testVertices;

    bool animStart = false; 

    Image image; 

    enum AnimationType {
    NONE,
    ANIM1,
    ANIM2,
    ANIM3,
    ANIM4 
    };

    AnimationType currentAnimation = NONE;
    double timeSinceAnimStart = 0;
    double animDuration = 0;

    void onInit() override{

        auto hasLoaded = image.load("../photo.png"); 
        if (!hasLoaded) {
            std::cout << "Image not found" << std::endl;
            exit(1);
        }

    }

    void onCreate() override {

        nav().pos(0, 0, 5);

        if (! shader.compile(slurp("../point-vertex.glsl"), slurp("../point-fragment.glsl"), slurp("../point-geometry.glsl"))) {
            printf("Shader failed to compile\n");
            exit(1);
        }


        mesh.primitive(Mesh::POINTS);
        grid.primitive(Mesh::POINTS);
        rgb.primitive(Mesh::POINTS);
        test.primitive(Mesh::POINTS);
        hsv.primitive(Mesh::POINTS);
        
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                auto pixel = image.at(x, y);
                mesh.vertex(float(x) / image.width(), float(-y) / image.width(), 0);
                mesh.color(pixel.r / 255.0, pixel.g / 255.0, pixel.b / 255.0);
                mesh.texCoord(0.5, 0);

                grid.vertex(float(x) / image.width(), float(-y) / image.width(), 0);
                grid.color(pixel.r / 255.0, pixel.g / 255.0, pixel.b / 255.0);
                grid.texCoord(0.5, 0);

                float r = pixel.r / 255.0;
                float g = pixel.g / 255.0;
                float b = pixel.b / 255.0; 

                rgb.vertex(r, g, b);
                rgb.color(r, g, b);
                rgb.texCoord(0.5, 0);

                test.vertex(float(x) / image.width(), float(y) / image.width(), 0);
                test.color(pixel.r / 255.0, pixel.g / 255.0, pixel.b / 255.0);
                test.texCoord(0.5, 0);

                HSV hsvPoints(RGB(r, g, b));
                // Convert HSV to XYZ coordinates on a cylinder
                float angle = hsvPoints.h * 2.0f * M_PI; // Convert hue to radians
                float radius = hsvPoints.s; // Use saturation as radius
                float height = hsvPoints.v; // Use value as height
                
                // Convert polar coordinates to cartesian
                float x_coord = radius * cos(angle);
                float y_coord = radius * sin(angle);
                float z_coord = height;
                
                hsv.vertex(x_coord, y_coord, z_coord);


            }

      
        }

        meshVertices = mesh.vertices();
        gridVertices = grid.vertices();
        rgbVertices = rgb.vertices();   
        testVertices = test.vertices();
        hsvVertices = hsv.vertices();
    }

    double time = 0;


    void onAnimate(double dt) override {
    if (currentAnimation != NONE) {
        timeSinceAnimStart += dt;

        float t = timeSinceAnimStart / animDuration;
        if (t > 1.0f) {
            currentAnimation = NONE;
            return;
        }

        for (int i = 0; i < mesh.vertices().size(); ++i) {
            Vec3f a = meshVertices[i];  // start
            Vec3f b;
            
            if (currentAnimation == ANIM1) {
                b = gridVertices[i];
            } else if (currentAnimation == ANIM2) {
                b = hsvVertices[i];
            } else if (currentAnimation == ANIM3) {
                b = rgbVertices[i];
            } else if (currentAnimation == ANIM4) {
                b = testVertices[i];
            }

            Vec3f c = a * (1.0f - t) + b * t;
            mesh.vertices()[i] = c;
        }
    }
}


    void onDraw(Graphics& g) override {
        g.clear(0.1);
        g.shader(shader);
        g.shader().uniform("pointSize", pointSize);
        g.blending(true);
        g.blendTrans();
        g.depthTesting(true);
        g.draw(mesh);
    }

    bool onKeyDown(const Keyboard& k) override {
        if (k.key() == 'q') {
            std::cout << "Exiting..." << std::endl;
            quit();
        }

        meshVertices = mesh.vertices();        

        if (k.key() == '1') {
            currentAnimation = ANIM1;
            animDuration = 1.0;
            timeSinceAnimStart = 0;
        }

        if (k.key() == '2') {
            currentAnimation = ANIM2;
            animDuration = 2.0;
            timeSinceAnimStart = 0;
        }

        if (k.key() == '3') {
            currentAnimation = ANIM3;
            animDuration = 3.0;
            timeSinceAnimStart = 0;
        }

        if (k.key() == '4') {
            currentAnimation = ANIM4;
            animDuration = 4.0;
            timeSinceAnimStart = 0;
        }

        return true;
    }

}; 



int main() { MyApp().start(); }

std::string slurp(std::string fileName) {
    std::fstream file(fileName);
    std::string returnValue = "";
    while (file.good()) {
      std::string line;
      getline(file, line);
      returnValue += line + "\n";
    }
    return returnValue;
  }