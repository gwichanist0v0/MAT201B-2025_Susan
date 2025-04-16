#include "al/app/al_App.hpp"
#include "al/graphics/al_Image.hpp" 
#include "al/graphics/al_Shapes.hpp" // addCone, addCube, addSphere
#include "al/types/al_Color.hpp"
#include "al/math/al_Interpolation.hpp"

using namespace al;

#include <fstream> // for slurp()
#include <string> // for slurp()

Vec3f rvec() { return Vec3f(1.0, 1.0, 1.0);}
RGB rcolor() { return RGB (1.0, 1.0, 1.0); }

std::string slurp(std::string fileName); // only a declaration


class MyApp : public App{

    Mesh grid, rgb, hsv, mine;
    Mesh mesh; 
    ShaderProgram shader;
    Parameter pointSize{"pointSize", 0.005, 0.005, 0.005 };

    Mesh::Vertices meshVertices;
    Mesh::Vertices gridVertices; 
    Mesh::Vertices rgbVertices;


    Image image; 

    void onInit() override{

        auto hasLoaded = image.load("../photo.png"); 
        if (!hasLoaded) {
            std::cout << "Image not found" << std::endl;
            exit(1);
        }

    }

    void onCreate() override {

        //makeImage(); 

        nav().pos(0, 0, 5);

        if (! shader.compile(slurp("../point-vertex.glsl"), slurp("../point-fragment.glsl"), slurp("../point-geometry.glsl"))) {
            printf("Shader failed to compile\n");
            exit(1);
        }


        mesh.primitive(Mesh::POINTS);
        grid.primitive(Mesh::POINTS);
        rgb.primitive(Mesh::POINTS);

        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                auto pixel = image.at(x, y);
                mesh.vertex(float(x) / image.width(), float(-y) / image.width(), 0);
                mesh.color(pixel.r / 255.0, pixel.g / 255.0, pixel.b / 255.0);
                mesh.texCoord(0.1, 0);

                grid.vertex(float(x) / image.width(), float(-y) / image.width(), 0);
                grid.color(pixel.r / 255.0, pixel.g / 255.0, pixel.b / 255.0);
                grid.texCoord(0.1, 0);

                float r = pixel.r / 255.0;
                float g = pixel.g / 255.0;
                float b = pixel.b / 255.0; 

                rgb.vertex(r, g, b);
                rgb.color(r, g, b);
                rgb.texCoord(0.1, 0);

                HSV foo(RGB(1, 0, 1));


            }

      
        }

        meshVertices = mesh.vertices();
        gridVertices = grid.vertices();
        rgbVertices = rgb.vertices();

    }

    double time = 0;

    void onAnimate(double dt) override {
        time += dt;

    
        if (time < 2 && time > 1) {
         
            std::cout << time << std::endl;  


            // std::cout << meshVertices.size() << std::endl;  
            // std::cout << rgbVertices.size() << std::endl;  

            for (int i = 0; i < meshVertices.size(); ++i) {
                
                auto endVertices = ipl::linear(time, meshVertices[i], rgbVertices[i]); 
                mesh.vertex(endVertices); 



                // std::cout << meshVertices[i] << std::endl;  
                // std::cout << rgbVertices[i] << std::endl;  
                
                //meshVertice[i] ... rgb grid etc 


                
            }

        }
        //update `mesh` to be a mix of two other meshes
        // for each vertex of `mesh`
        //   vertex = mix(A, B)
        //


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


        if (k.key() == '1') {
            time = 0;
        }

        if (k.key() == '2'){
            
            time = 1; 

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