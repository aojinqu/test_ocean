
#pragma once

#include <ToyGraph/Scene/Scene.h>
#include <ToyGraph/Skybox.h>
#include <ToyGraph/Camera.h>
#include <ToyGraph/Actor.h>
#include <ToyEffects/scenes/Skybox/gl4ext.h>
//#include <ToyEffects/scenes/Skybox/terrainquadtree.h>
#include <vector>


class WaterScene : public Scene {
public:
	//变量
	int cnt;		//暂定
    float dT;       //运行的时间
	//函数
    ~WaterScene();

    static WaterScene* constructor() {
        return new WaterScene;
    }

    virtual void render() override;
    virtual void tick(float deltaT) override;

    WaterScene();

    void cursorPosCallback(double xPos, double yPos) override;

    void activeKeyInputProcessor(GLFWwindow* window, float deltaT) override;

    bool InitScene();
	void UninitScene();
	void GenerateLODLevels(OpenGLAttributeRange** subsettable, unsigned int* numsubsets, uint32_t* idata);
	unsigned int GenerateBoundaryMesh(int deg_left, int deg_top, int deg_right, int deg_bottom, int levelsize, uint32_t* idata);
    float Phillips(const glm::vec2& k, const glm::vec2& w, float V, float A);
    void FourierTransform(GLuint spectrum);
    void RenderOcean(float alpha, float elapsedtime);

    Skybox* pSkybox = nullptr;
    

    //画个cube
    Shader cube{
        "shaders/cube.vs",
        "shaders/cube.fs"
    };

    Shader water_needs[8];
};

