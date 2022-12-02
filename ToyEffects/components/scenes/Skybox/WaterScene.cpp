
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "GLU32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdiplus.lib")


#include <ToyEffects/scenes/Skybox/WaterScene.h>
#include <ToyEffects/scenes/Skybox/shared.h>
#include <ToyEffects/scenes/Skybox/terrainquadtree.h>

#include <iostream>
#include <random>
#include <complex>
#include <fstream>
using namespace std;


//�궨��
#define DISP_MAP_SIZE		512					// 1024 ������ɵ�����
#define PATCH_SIZE			20.0f				// m
#define WIND_DIRECTION		{ -0.4f, -0.9f }	//����������
#define WIND_SPEED			6.5f				// m/s  ����
#define AMPLITUDE_CONSTANT	(0.45f * 1e-3f)		// Phillips ���׵�A
#define GRAV_ACCELERATION	9.81f				// m/s^2 ���ٶ�
#define MESH_SIZE			64					// [64, 256]
#define FURTHEST_COVER		8					// ����������= PATCH_SIZE * (1 << FURTHEST_COVER)
#define MAX_COVERAGE		64.0f				// pixel limit for a distant patch to be rendered
//Ϊopengl��չapi��glet.h�е����ݣ�����ɾ
//#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84ff
//#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE

vector<unsigned int> p_id;
OpenGLMesh* oceanmesh = nullptr;

OpenGLEffect* debugeffect = nullptr;
OpenGLEffect* updatespectrum = nullptr;
OpenGLEffect* fourier_dft = nullptr;	// bruteforce solution
OpenGLEffect* fourier_fft = nullptr;	// fast fourier transform
OpenGLEffect* createdisp = nullptr;	// displacement
OpenGLEffect* creategrad = nullptr;	// normal & jacobian
OpenGLEffect* oceaneffect = nullptr;
OpenGLEffect* wireeffect = nullptr;
//OpenGLEffect* skyeffect = nullptr;

OpenGLScreenQuad* screenquad = nullptr;

TerrainQuadTree		tree;

static const int IndexCounts[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	961920,		// 64x64
	3705084,	// 128x128
	14500728	// 256x256
};
float square_vertices[] = {
	// positions          // normals           // texture coords
	// ��һ��������
	0.5f, 0.5f, 0.0f,   // ���Ͻ�
	0.5f, -0.5f, 0.0f,  // ���½�
	-0.5f, 0.5f, 0.0f,  // ���Ͻ�
	// �ڶ���������
	0.5f, -0.5f, 0.0f,  // ���½�
	-0.5f, -0.5f, 0.0f, // ���½�
	-0.5f, 0.5f, 0.0f   // ���Ͻ�

};
unsigned int VBO, VAO;


//��ѧ����
static const float PI = 3.141592f;
static const float ONE_OVER_SQRT_2 = 0.7071067f;	//����2��֮1
static const float ONE_OVER_SQRT_TWO_PI = 0.39894228f;
//��д��ȫ�ֱ���������water�����ٷ�װ
unsigned int				initial = 0;			// ��ʼ����,h0(����)
unsigned int				frequencies = 0;		// Ƶ�� w_i ÿ��������
unsigned int				updated[2] = { 0 };		// updated spectra h~(k,t)��D~(k,t)
unsigned int				tempdata = 0;			// �м���� FT
unsigned int				displacement = 0;		// λ��ͼ
unsigned int				gradients = 0;			// �����۵���ͼ
uint32_t					numlods = 0;
unsigned int				perlintex = 0;		// Perlin ���� to remove tiling artifacts
unsigned int				environment = 0;
unsigned int				debugvao = 0;
unsigned int				helptext = 0;


//��wn��׼��
void Vec2Normalize(glm::vec2& out, glm::vec2& v);
//���������ȣ����滻��
float Vec2Length(const glm::vec2& v);
//��log2x������
uint32_t Log2OfPow2(uint32_t x);
static GLuint CalcSubsetIndex(int level, int dL, int dR, int dB, int dT);

//��������д����ʵӦ�õ���
void WaterScene::cursorPosCallback(double xPos, double yPos) {
    __nahidaPaimonSharedCursorPosCallback(xPos, yPos);
}

void WaterScene::activeKeyInputProcessor(GLFWwindow* window, float deltaTime) {
    __nahidaPaimonSharedActiveKeyInputProcessor(window, deltaTime);
	dT = deltaTime;
}


WaterScene::~WaterScene() {
    if (this->pSkybox) {
        delete this->pSkybox;
    }

}
//ʱ�̸ı�λ�ã����ﲻ��Ҫ
void WaterScene::tick(float deltaT) {
    auto ocean_test = this->actors[0];
	dT = deltaT;
    //ocean_test->setYaw(ocean_test->getYaw() + deltaT * 20);
}


void WaterScene::render() {
    auto& runtime = AppRuntime::getInstance();

    pSkybox->render();

	RenderOcean(0.1f, dT / 10000);

	for (int i = 0; i < p_id.size(); i++)
		water_needs[i].use();

    //cube.use();
    //auto view = camera->getViewMatrix();	
    //auto projection = glm::perspective(
    //    glm::radians(camera->getFov()),
    //    1.0f * runtime.getWindowWidth() / runtime.getWindowHeight(),
    //    0.1f,
    //    100.0f
    //);
    //glm::mat4 model = glm::mat4(1.0f);
    //model = glm::rotate(model, glm::radians(45.0f), glm::vec3(1.0f, 0.3f, 0.5f));
    //model = glm::scale(model, glm::vec3(0.2f)); // Make it a smaller cube
    //cube.setMatrix4fv("projection", projection)
    //    .setMatrix4fv("view", view)
    //    .setMatrix4fv("model", model);

    //glBindVertexArray(VAO);
	//glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDrawArrays(GL_TRIANGLES, 0, 4);

    for (auto it : this->actors) {
        it.second->render(&cube);
    }

	
}

WaterScene::WaterScene() {

	vector<string> skyboxFaces({
		"assets/SpaceboxCollection/Spacebox3/LightGreen_right1.png",
		"assets/SpaceboxCollection/Spacebox3/LightGreen_left2.png",
		"assets/SpaceboxCollection/Spacebox3/LightGreen_top3.png",
		"assets/SpaceboxCollection/Spacebox3/LightGreen_bottom4.png",
		"assets/SpaceboxCollection/Spacebox3/LightGreen_front5.png",
		"assets/SpaceboxCollection/Spacebox3/LightGreen_back6.png"
		});

	pSkybox = new Skybox(skyboxFaces);

	Actor* ocean_test = new Actor;
	ocean_test->setScale(glm::vec3(1.0));
	this->addActor(ocean_test);

	for (int i = 0; i < p_id.size(); i++)
		water_needs[i].setId(i);

	// ��Ⱦһ�������ʵ�������,����һЩ����
	//glGenVertexArrays(1, &VAO);
	//glGenBuffers(1, &VBO);
	////���ɲ���VAO��VBO
	//glBindVertexArray(VAO);
	//glBindBuffer(GL_ARRAY_BUFFER, VBO);
	//// ���������ݰ�����ǰĬ�ϵĻ�����
	//glBufferData(GL_ARRAY_BUFFER, sizeof(square_vertices), square_vertices, GL_STATIC_DRAW);
	//// ���ö�������ָ��
	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	//glEnableVertexAttribArray(0);
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	//glEnableVertexAttribArray(1);
	//glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	//glEnableVertexAttribArray(2);
	this->camera = new Camera;
	//camera->setPosition(glm::vec3(-10, 0, 0));

	InitScene();
	UninitScene();
	//if (cube.errcode != ShaderError::SHADER_OK) {
	//	cout << "cube shader err: " << cube.errmsg << endl;
	//}



	cnt = 0;
}

bool WaterScene::InitScene()
{
	auto& runtime = AppRuntime::getInstance();

	std::mt19937 gen;//�����
	std::normal_distribution<> gaussian(0.0, 1.0);//��̬�ֲ�������0������1
	GLint maxanisotropy = 1;		//�������ԣ�ʲô����..

	uint32_t screenwidth = runtime.getWindowWidth();
	uint32_t screenheight = runtime.getWindowHeight();


	// setup OpenGL
	glClearColor(0.0f, 0.125f, 0.3f, 1.0f);
	glClearDepth(1.0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);

	//ͼԪ����(Primitive restart) �����û����Ʋ������ġ���ɢ��ͼ��
	//�������ֵ��ʱ��OpenGL�������ͼԪ�����ǽ�����һ�λ��ƣ�Ȼ�����������µĻ���
	glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

	glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxanisotropy);
	maxanisotropy = max(maxanisotropy, 2);

	// ���ɳ�ʼƵ�׺�Ƶ��
	glm::vec2 k;
	float L = PATCH_SIZE;

	glGenTextures(1, &initial);
	glGenTextures(1, &frequencies);

	glBindTexture(GL_TEXTURE_2D, initial);
	//���������ͼ��
	//������ƻὫָ�� ����ͼ�� ��һ����ӳ�䵽���������ÿ��ͼ�λ�Ԫ
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG32F, DISP_MAP_SIZE + 1, DISP_MAP_SIZE + 1);

	glBindTexture(GL_TEXTURE_2D, frequencies);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, DISP_MAP_SIZE + 1, DISP_MAP_SIZE + 1);

	// n��[-N / 2, N / 2]��
	int start = DISP_MAP_SIZE / 2;

	// Ϊ�˶Գ�,  (N + 1) x (N + 1) 
	//����һ����������,�洢����̬�ֲ�������������h0(k)������
	complex<float>* h0data = new complex<float>[(DISP_MAP_SIZE + 1) * (DISP_MAP_SIZE + 1)];

	float* wdata = new float[(DISP_MAP_SIZE + 1) * (DISP_MAP_SIZE + 1)];

	glm::vec2 w = WIND_DIRECTION;		//������
	glm::vec2 wn;
	float V = WIND_SPEED;				//����
	float A = AMPLITUDE_CONSTANT;		//PHilipƵ�׵�A

	//cout << "����߶ȳ�����ӡǰ50��" << endl;
	//��w��׼����wn,w�Ȳ������������ﲻ�ÿ⺯��
	Vec2Normalize(wn, w);
	//����m,n���������е�w��h0ֵ���̶�ֵ
	for (int m = 0; m <= DISP_MAP_SIZE; ++m)
	{
		k.y = ((2 * PI) * (start - m)) / L;

		for (int n = 0; n <= DISP_MAP_SIZE; ++n) {
			k.x = ((2 * PI) * (start - n)) / L;

			int index = m * (DISP_MAP_SIZE + 1) + n;
			float sqrt_P_h = 0;

			if (k.x != 0.0f || k.y != 0.0f)
				sqrt_P_h = sqrtf(Phillips(k, wn, V, A));

			h0data[index].real((float)(sqrt_P_h * gaussian(gen) * ONE_OVER_SQRT_2));
			h0data[index].imag((float)(sqrt_P_h * gaussian(gen) * ONE_OVER_SQRT_2));
			// ��������� w^2(k) = gk
			wdata[index] = sqrtf(GRAV_ACCELERATION * Vec2Length(k));
			//ֻ��ӡһ��
			if (cnt <= 50) {
				cout << "m:" << m << "    " << "n:" << n << "    ";
				cout << "h~0(m,n):" << h0data[index].real() << "+" << h0data[index].imag() << "i" << endl;
				cout << "index: " << index << "    ������w2(k):" << wdata[index] << endl << endl;;
				cnt++;
			}


		}
	}


	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DISP_MAP_SIZE + 1, DISP_MAP_SIZE + 1, GL_RED, GL_FLOAT, wdata);

	glBindTexture(GL_TEXTURE_2D, initial);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DISP_MAP_SIZE + 1, DISP_MAP_SIZE + 1, GL_RG, GL_FLOAT, h0data);

	delete[] wdata;
	delete[] h0data;

	// ��������Ƶ����ͼ
	glGenTextures(2, updated);
	glBindTexture(GL_TEXTURE_2D, updated[0]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG32F, DISP_MAP_SIZE, DISP_MAP_SIZE);

	glBindTexture(GL_TEXTURE_2D, updated[1]);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG32F, DISP_MAP_SIZE, DISP_MAP_SIZE);

	glGenTextures(1, &tempdata);
	glBindTexture(GL_TEXTURE_2D, tempdata);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RG32F, DISP_MAP_SIZE, DISP_MAP_SIZE);

	// λ��ͼ��ʲô��˼����
	glGenTextures(1, &displacement);
	glBindTexture(GL_TEXTURE_2D, displacement);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, DISP_MAP_SIZE, DISP_MAP_SIZE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// create gradient & folding map
	glGenTextures(1, &gradients);
	glBindTexture(GL_TEXTURE_2D, gradients);
	glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA16F, DISP_MAP_SIZE, DISP_MAP_SIZE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxanisotropy / 2);

	glBindTexture(GL_TEXTURE_2D, 0);

	// Shader LOD��ʵ���Ǹ����豸���ܵĲ�ͬ���벻ͬ�汾��Shader
	// ���������LOD����(������δ��ʹ��tess shader),,������
	OpenGLVertexElement decl[] = {
		{ 0, 0, GLDECLTYPE_FLOAT3, GLDECLUSAGE_POSITION, 0 },
		{ 0xff, 0, 0, 0, 0 }
	};

	numlods = Log2OfPow2(MESH_SIZE);

	if (!GLCreateMesh((MESH_SIZE + 1) * (MESH_SIZE + 1), IndexCounts[numlods], GLMESH_32BIT, decl, &oceanmesh))
		return false;

	OpenGLAttributeRange* subsettable = nullptr;
	glm::vec3* vdata = nullptr;
	uint32_t* idata = nullptr;
	unsigned int numsubsets = 0;

	oceanmesh->LockVertexBuffer(0, 0, GLLOCK_DISCARD, (void**)&vdata);
	oceanmesh->LockIndexBuffer(0, 0, GLLOCK_DISCARD, (void**)&idata);
		float tilesize = PATCH_SIZE / MESH_SIZE;

		// vertex data
		for (int z = 0; z <= MESH_SIZE; ++z) {
			for (int x = 0; x <= MESH_SIZE; ++x) {
				int index = z * (MESH_SIZE + 1) + x;

				vdata[index].x = (float)x;
				vdata[index].y = (float)z;
				vdata[index].z = 0.0f;
			}
		}

			// index data
			GenerateLODLevels(&subsettable, &numsubsets, idata);
		
		oceanmesh->UnlockIndexBuffer();
		oceanmesh->UnlockVertexBuffer();

		oceanmesh->SetAttributeTable(subsettable, numsubsets);
		delete[] subsettable;

		// load shaders
		char defines[128];

		sprintf_s(defines, "#define DISP_MAP_SIZE	%d\n#define LOG2_DISP_MAP_SIZE	%d\n#define TILE_SIZE_X2	%.4f\n#define INV_TILE_SIZE	%.4f\n",
			DISP_MAP_SIZE,
			Log2OfPow2(DISP_MAP_SIZE),
			PATCH_SIZE * 2.0f / DISP_MAP_SIZE,
			DISP_MAP_SIZE / PATCH_SIZE);

		//test�ļ�·����д�Ƿ���ȷ
		//string* str = ;
		//fstream fff("shaders/ocean/updatespectrum.comp", fstream::in);
		//if (!fff.is_open())
		//{
		//	cout << "no!!";
		//	exit(0);
		//}
		//else
		//	cout << "yes";


		if (!GLCreateEffectFromFile("shaders/ocean/screenquad.vert", 0, 0, 0, "shaders/ocean/debugspectra.frag", &debugeffect, defines))
			return false;

		if (!GLCreateComputeProgramFromFile("shaders/ocean/updatespectrum.comp", &updatespectrum, defines))
			return false;

		if (!GLCreateComputeProgramFromFile("shaders/ocean/fourier_dft.comp", &fourier_dft, defines))
			return false;

		if (!GLCreateComputeProgramFromFile("shaders/ocean/fourier_fft.comp", &fourier_fft, defines))
			return false;

		if (!GLCreateComputeProgramFromFile("shaders/ocean/createdisplacement.comp", &createdisp, defines))
			return false;

		if (!GLCreateComputeProgramFromFile("shaders/ocean/creategradients.comp", &creategrad, defines))
			return false;

		if (!GLCreateEffectFromFile("shaders/ocean/ocean.vert", 0, 0, 0, "shaders/ocean/ocean.frag", &oceaneffect, defines))
			return false;

		if (!GLCreateEffectFromFile("shaders/ocean/ocean.vert", 0, 0, 0, "shaders/ocean/simplecolor.frag", &wireeffect, defines))
			return false;

		p_id.push_back(debugeffect->GetProgramId());
		p_id.push_back(updatespectrum->GetProgramId());
		p_id.push_back(fourier_dft->GetProgramId());
		p_id.push_back(fourier_fft->GetProgramId());
		p_id.push_back(createdisp->GetProgramId());
		p_id.push_back(creategrad->GetProgramId());
		p_id.push_back(oceaneffect->GetProgramId());
		p_id.push_back(wireeffect->GetProgramId());

		screenquad = new OpenGLScreenQuad();

		// NOTE: can't query image bindings
		updatespectrum->SetInt("tilde_h0", 0);
		updatespectrum->SetInt("frequencies", 1);
		updatespectrum->SetInt("tilde_h", 2);
		updatespectrum->SetInt("tilde_D", 3);

		fourier_dft->SetInt("readbuff", 0);
		fourier_dft->SetInt("writebuff", 1);

		fourier_fft->SetInt("readbuff", 0);
		fourier_fft->SetInt("writebuff", 1);

		createdisp->SetInt("heightmap", 0);
		createdisp->SetInt("choppyfield", 1);
		createdisp->SetInt("displacement", 2);

		creategrad->SetInt("displacement", 0);
		creategrad->SetInt("gradients", 1);

		oceaneffect->SetInt("displacement", 0);
		oceaneffect->SetInt("perlin", 1);
		oceaneffect->SetInt("envmap", 2);
		oceaneffect->SetInt("gradients", 3);

		float white[] = { 1, 1, 1, 1 };

		wireeffect->SetInt("displacement", 0);
		wireeffect->SetInt("perlin", 1);
		wireeffect->SetVector("matColor", white);

		// skyeffect->SetInt("sampler0", 0);
		debugeffect->SetInt("sampler0", 0);

		// other
		if (!GLCreateTextureFromFile("textures/perlin_noise.dds", false, &perlintex))
			return false;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxanisotropy / 2);


		if (!GLCreateCubeTextureFromDDS("textures/ocean_env.dds", true, &environment))
			return false;

		glGenVertexArrays(1, &debugvao);
		glBindVertexArray(debugvao);
		{
			// empty
		}
		glBindVertexArray(0);

		// init quadtree
		float ocean_extent = PATCH_SIZE * (1 << FURTHEST_COVER);
		float ocean_start[2] = { -0.5f * ocean_extent, -0.5f * ocean_extent };

		tree.Initialize(ocean_start, ocean_extent, (int)numlods, MESH_SIZE, PATCH_SIZE, MAX_COVERAGE, (float)(screenwidth * screenheight));

		 //render text
		//GLCreateTexture(512, 512, 1, GLFMT_A8B8G8R8, &helptext);

		//GLRenderText(
		//	"Use WASD and mouse to move around\n1-8: Change ocean color\n\nT - Toggle FFT/DFT\nR - Toggle debug camera\nH - Toggle help text",
		//	helptext, 512, 512);

		return true;
}

void WaterScene::UninitScene()
{
	delete oceanmesh;
	//delete skyeffect;
	delete oceaneffect;
	delete wireeffect;
	delete debugeffect;
	delete updatespectrum;
	delete fourier_dft;
	delete fourier_fft;
	delete createdisp;
	delete creategrad;
	delete screenquad;

	glDeleteVertexArrays(1, &debugvao);

	glDeleteTextures(1, &displacement);
	glDeleteTextures(1, &gradients);
	glDeleteTextures(1, &initial);
	glDeleteTextures(1, &frequencies);
	glDeleteTextures(2, updated);
	glDeleteTextures(1, &tempdata);
	glDeleteTextures(1, &environment);
	glDeleteTextures(1, &perlintex);
	glDeleteTextures(1, &helptext);

	OpenGLContentManager().Release();



}


void WaterScene::RenderOcean(float alpha, float elapsedtime)
{
	auto& runtime = AppRuntime::getInstance();

	static float time = 0.0f;

	Math::Matrix world, view;
	//Math::Matrix viewproj, debugviewproj;
	//Math::Vector3 eye, debugeye;

	uint32_t screenwidth = runtime.getWindowWidth();
	uint32_t screenheight = runtime.getWindowHeight();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, screenwidth, screenheight);

	//if (use_debug) {
	//	debugcamera.Animate(alpha);
	//	debugcamera.GetViewMatrix(view);
	//	debugcamera.GetProjectionMatrix(proj);
	//	debugcamera.GetEyePosition(debugeye);

	//	Math::MatrixMultiply(debugviewproj, view, proj);
	//}
	auto mid = camera->getViewMatrix();
	mid;
	//view = Math::Matrix(camera->getViewMatrix());
	//camera.Animate(alpha);
	//camera.GetViewMatrix(view);
	//camera.GetProjectionMatrix(proj);
	//camera.GetEyePosition(eye);

	Math::Vector3 eye(0.0f, 15.0f, 0.0f);
	Math::Matrix viewproj;
	Math::Matrix proj;
	//��������Ҫ�滻proj��
	//auto projection = glm::perspective(
	//	glm::radians(camera->getFov()),
	//	1.0f * runtime.getWindowWidth() / runtime.getWindowHeight(),
	//	0.1f,
	//	100.0f
	//);
	Math::MatrixMultiply(viewproj, view, proj);

	// update spectra
	updatespectrum->SetFloat("time", time);
	//����bind�����⣡������������������������������������������������
	glBindImageTexture(0, initial, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RG32F);
	glBindImageTexture(1, frequencies, 0, GL_TRUE, 0, GL_READ_ONLY, GL_R32F);

	glBindImageTexture(2, updated[0], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RG32F);
	glBindImageTexture(3, updated[1], 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RG32F);

	//�����ʹ�õ��ԣ����д�
	//GLenum err = glGetError();
	//if (err != GL_NO_ERROR)
	//{
	//	std::cout << "Error\n" << err;
	//	exit(0);
	//}

	//��render�����в����ٸ�effect����program�ˡ�
	updatespectrum->Begin();
	{
		glDispatchCompute(DISP_MAP_SIZE / 16, DISP_MAP_SIZE / 16, 1);
	}
	updatespectrum->End();

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// transform spectra to spatial/time domain
	FourierTransform(updated[0]);
	FourierTransform(updated[1]);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// calculate displacement map

	glBindImageTexture(0, updated[0], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RG32F);
	glBindImageTexture(1, updated[1], 0, GL_TRUE, 0, GL_READ_ONLY, GL_RG32F);
	glBindImageTexture(2, displacement, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	createdisp->Begin();
	{
		glDispatchCompute(DISP_MAP_SIZE / 16, DISP_MAP_SIZE / 16, 1);
	}
	createdisp->End();

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// calculate normal & folding map
	glBindImageTexture(0, displacement, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
	glBindImageTexture(1, gradients, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);

	creategrad->Begin();
	{
		glDispatchCompute(DISP_MAP_SIZE / 16, DISP_MAP_SIZE / 16, 1);
	}
	creategrad->End();

	glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

	glBindTexture(GL_TEXTURE_2D, gradients);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	// render sky
	//glEnable(GL_FRAMEBUFFER_SRGB);

	Math::MatrixScaling(world, 5.0f, 5.0f, 5.0f);

	//world._41 = eye.x;
	//world._42 = eye.y;
	//world._43 = eye.z;

	//skyeffect->SetMatrix("matWorld", world);
	//skyeffect->SetMatrix("matViewProj", viewproj);
	//skyeffect->SetVector("eyePos", eye);
	//if (!use_debug) {
	//	skyeffect->Begin();
	//	{
	//		glDepthMask(GL_FALSE);
	//		glBindTexture(GL_TEXTURE_CUBE_MAP, environment);
	//		skymesh->Draw();
	//		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	//		glDepthMask(GL_TRUE);
	//	}
	//	skyeffect->End();
	//}

	// build quadtree
	//����һ�����壬����ʱ�ָ�ռ�ֱ�������ɸ��������С�ռ䣬��������嵽���
	tree.Rebuild(viewproj, proj, eye);

	// render ocean
	Math::Matrix	flipYZ = { 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1 };
	Math::Matrix	localtraf;
	Math::Vector4	uvparams = { 0, 0, 0, 0 };
	Math::Vector4	perlinoffset = { 0, 0, 0, 0 };
	Math::Vector2	w = WIND_DIRECTION;
	int				pattern[4];
	//OpenGLEffect* effect = (use_debug ? wireeffect : oceaneffect);
	OpenGLEffect* effect = oceaneffect;

	GLuint			subset = 0;

	uvparams.x = 1.0f / PATCH_SIZE;
	uvparams.y = 0.5f / DISP_MAP_SIZE;

	perlinoffset.x = -w.x * time * 0.06f;
	perlinoffset.y = -w.y * time * 0.06f;

	Math::Color ocean_color = { 0.0056f, 0.0194f, 0.0331f, 1 };
	oceaneffect->SetMatrix("matViewProj", viewproj);
	oceaneffect->SetVector("perlinOffset", perlinoffset);
	oceaneffect->SetVector("eyePos", eye);
	oceaneffect->SetVector("oceanColor", ocean_color);	// deep blue

	//wireeffect->SetMatrix("matViewProj", debugviewproj);
	//wireeffect->SetVector("perlinOffset", perlinoffset);
	//wireeffect->SetVector("eyePos", debugeye);

	//if (use_debug)
	//	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		// check errors



	effect->Begin();
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, displacement);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, perlintex);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environment);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, gradients);



	tree.Traverse([&](const TerrainQuadTree::Node& node) {
		float levelsize = (float)(MESH_SIZE >> node.lod);
		float scale = node.length / levelsize;

		Math::MatrixScaling(localtraf, scale, scale, 0);

		Math::MatrixTranslation(world, node.start[0], 0, node.start[1]);
		Math::MatrixMultiply(world, flipYZ, world);

		uvparams.z = node.start[0] / PATCH_SIZE;
		uvparams.w = node.start[1] / PATCH_SIZE;

		effect->SetMatrix("matLocal", localtraf);
		effect->SetMatrix("matWorld", world);
		effect->SetVector("uvParams", uvparams);
		effect->CommitChanges();

		tree.FindSubsetPattern(pattern, node);
		subset = CalcSubsetIndex(node.lod, pattern[0], pattern[1], pattern[2], pattern[3]);

		if (subset < oceanmesh->GetNumSubsets() - 1) {
			oceanmesh->DrawSubset(subset);
			oceanmesh->DrawSubset(subset + 1);
		}
		});

	glActiveTexture(GL_TEXTURE0);



	effect->End();

	//if (use_debug)
	//	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glDisable(GL_FRAMEBUFFER_SRGB);
	time += elapsedtime;

	// TODO: draw spectra

	//if (drawtext) {
	//	// render text
	//	glViewport(10, screenheight - 522, 512, 512);
	//	glEnable(GL_BLEND);
	//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//	glDisable(GL_DEPTH_TEST);

	//	Math::Vector4 xzplane = { 0, 1, 0, -0.5f };
	//	Math::MatrixReflect(world, xzplane);

	//	glActiveTexture(GL_TEXTURE0);
	//	glBindTexture(GL_TEXTURE_2D, helptext);

	//	screenquad->SetTextureMatrix(world);
	//	screenquad->Draw();
	//}

	// reset states
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

}


// Phillips����
 float WaterScene::Phillips(const glm::vec2& k, const glm::vec2& w, float V, float A)
{
	float L = (V * V) / GRAV_ACCELERATION;	// largest possible wave for wind speed V
	float l = L / 1000.0f;					// supress waves smaller than this

	float kdotw =glm::dot(k, w);
	float k2 = glm::dot(k, k);			// squared length of wave vector k

	// k^6 because k must be normalized
	float P_h = A * (expf(-1.0f / (k2 * L * L))) / (k2 * k2 * k2) * (kdotw * kdotw);

	if (kdotw < 0.0f) {
		// wave is moving against wind direction w
		P_h *= 0.07f;
	}

	return P_h * expf(-k2 * l * l);
}

 void WaterScene::FourierTransform(GLuint spectrum)
 {
	 //OpenGLEffect* effect = (use_fft ? fourier_fft : fourier_dft);
	 OpenGLEffect* effect = fourier_fft;

	 // horizontal pass
	 glBindImageTexture(0, spectrum, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RG32F);
	 glBindImageTexture(1, tempdata, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RG32F);

	 effect->Begin();
	 {
		 glDispatchCompute(DISP_MAP_SIZE, 1, 1);
	 }
	 effect->End();

	 glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	 // vertical pass
	 glBindImageTexture(0, tempdata, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RG32F);
	 glBindImageTexture(1, spectrum, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RG32F);

	 effect->Begin();
	 {
		 glDispatchCompute(DISP_MAP_SIZE, 1, 1);
	 }
	 effect->End();
 }


 unsigned int WaterScene::GenerateBoundaryMesh(int deg_left, int deg_top, int deg_right, int deg_bottom, int levelsize, uint32_t* idata)
 {
#define CALC_BOUNDARY_INDEX(x, z) \
	((z) * (MESH_SIZE + 1) + (x))
	 // END

	 unsigned int numwritten = 0;

	 // top edge
	 if (deg_top < levelsize) {
		 int t_step = levelsize / deg_top;

		 for (int i = 0; i < levelsize; i += t_step) {
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(i, 0);
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + t_step / 2, 1);
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + t_step, 0);

			 for (int j = 0; j < t_step / 2; ++j) {
				 if (i == 0 && j == 0 && deg_left < levelsize)
					 continue;

				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i, 0);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + j, 1);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + j + 1, 1);
			 }

			 for (int j = t_step / 2; j < t_step; ++j) {
				 if (i == levelsize - t_step && j == t_step - 1 && deg_right < levelsize)
					 continue;

				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + t_step, 0);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + j, 1);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + j + 1, 1);
			 }
		 }
	 }

	 // left edge
	 if (deg_left < levelsize) {
		 int l_step = levelsize / deg_left;

		 for (int i = 0; i < levelsize; i += l_step) {
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(0, i);
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(0, i + l_step);
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(1, i + l_step / 2);

			 for (int j = 0; j < l_step / 2; ++j) {
				 if (i == 0 && j == 0 && deg_top < levelsize)
					 continue;

				 idata[numwritten++] = CALC_BOUNDARY_INDEX(0, i);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(1, i + j + 1);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(1, i + j);
			 }

			 for (int j = l_step / 2; j < l_step; ++j) {
				 if (i == levelsize - l_step && j == l_step - 1 && deg_bottom < levelsize)
					 continue;

				 idata[numwritten++] = CALC_BOUNDARY_INDEX(0, i + l_step);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(1, i + j + 1);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(1, i + j);
			 }
		 }
	 }

	 // right edge
	 if (deg_right < levelsize) {
		 int r_step = levelsize / deg_right;

		 for (int i = 0; i < levelsize; i += r_step) {
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize, i);
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize - 1, i + r_step / 2);
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize, i + r_step);

			 for (int j = 0; j < r_step / 2; ++j) {
				 if (i == 0 && j == 0 && deg_top < levelsize)
					 continue;

				 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize, i);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize - 1, i + j);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize - 1, i + j + 1);
			 }

			 for (int j = r_step / 2; j < r_step; ++j) {
				 if (i == levelsize - r_step && j == r_step - 1 && deg_bottom < levelsize)
					 continue;

				 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize, i + r_step);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize - 1, i + j);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(levelsize - 1, i + j + 1);
			 }
		 }
	 }

	 // bottom edge
	 if (deg_bottom < levelsize) {
		 int b_step = levelsize / deg_bottom;

		 for (int i = 0; i < levelsize; i += b_step) {
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(i, levelsize);
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + b_step, levelsize);
			 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + b_step / 2, levelsize - 1);

			 for (int j = 0; j < b_step / 2; ++j) {
				 if (i == 0 && j == 0 && deg_left < levelsize)
					 continue;

				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i, levelsize);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + j + 1, levelsize - 1);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + j, levelsize - 1);
			 }

			 for (int j = b_step / 2; j < b_step; ++j) {
				 if (i == levelsize - b_step && j == b_step - 1 && deg_right < levelsize)
					 continue;

				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + b_step, levelsize);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + j + 1, levelsize - 1);
				 idata[numwritten++] = CALC_BOUNDARY_INDEX(i + j, levelsize - 1);
			 }
		 }
	 }

	 return numwritten;
 }


int calculate_inner_index(int x,int z, int top, int left)
 {
	return ((top + (z)) * (MESH_SIZE + 1) + left + (x));
 }

 void WaterScene::GenerateLODLevels(OpenGLAttributeRange** subsettable, unsigned int* numsubsets, uint32_t* idata)
 {
	 assert(subsettable);
	 assert(numsubsets);

	 *numsubsets = (numlods - 2) * 3 * 3 * 3 * 3 * 2;
	 *subsettable = new OpenGLAttributeRange[*numsubsets];

	 int currsubset = 0;
	 unsigned int indexoffset = 0;
	 unsigned int numwritten = 0;
	 OpenGLAttributeRange* subset = 0;

	 //int numrestarts = 0;

	 for (uint32_t level = 0; level < numlods - 2; ++level) {
		 int levelsize = MESH_SIZE >> level;
		 int mindegree = levelsize >> 3;

		 for (int left_degree = levelsize; left_degree > mindegree; left_degree >>= 1) {
			 for (int right_degree = levelsize; right_degree > mindegree; right_degree >>= 1) {
				 for (int bottom_degree = levelsize; bottom_degree > mindegree; bottom_degree >>= 1) {
					 for (int top_degree = levelsize; top_degree > mindegree; top_degree >>= 1) {
						 int right = ((right_degree == levelsize) ? levelsize : levelsize - 1);
						 int left = ((left_degree == levelsize) ? 0 : 1);
						 int bottom = ((bottom_degree == levelsize) ? levelsize : levelsize - 1);
						 int top = ((top_degree == levelsize) ? 0 : 1);

						 // generate inner mesh (triangle strip)
						 int width = right - left;
						 int height = bottom - top;

						 numwritten = 0;

						 for (int z = 0; z < height; ++z) {
							 if ((z & 1) == 1) {
								 idata[numwritten++] = calculate_inner_index(0, z,top,left);
								 idata[numwritten++] = calculate_inner_index(0, z + 1,top, left);

								 for (int x = 0; x < width; ++x) {
									 idata[numwritten++] = calculate_inner_index(x + 1, z, top, left);
									 idata[numwritten++] = calculate_inner_index(x + 1, z + 1, top, left);
								 }

								 idata[numwritten++] = UINT32_MAX;
								 //++numrestarts;
							 }
							 else {
								 idata[numwritten++] = calculate_inner_index(width, z + 1, top, left);
								 idata[numwritten++] = calculate_inner_index(width, z, top, left);

								 for (int x = width - 1; x >= 0; --x) {
									 idata[numwritten++] = calculate_inner_index(x, z + 1, top, left);
									 idata[numwritten++] = calculate_inner_index(x, z, top, left);
								 }

								 idata[numwritten++] = UINT32_MAX;
								 //++numrestarts;
							 }
						 }

						 // add inner subset
						 subset = ((*subsettable) + currsubset);

						 subset->AttribId = currsubset;
						 subset->Enabled = (numwritten > 0);
						 subset->IndexCount = numwritten;
						 subset->IndexStart = indexoffset;
						 subset->PrimitiveType = GL_TRIANGLE_STRIP;
						 subset->VertexCount = 0;
						 subset->VertexStart = 0;

						 indexoffset += numwritten;
						 idata += numwritten;

						 ++currsubset;

						 // generate boundary mesh (triangle list)
						 numwritten = GenerateBoundaryMesh(left_degree, top_degree, right_degree, bottom_degree, levelsize, idata);

						 // add boundary subset
						 subset = ((*subsettable) + currsubset);

						 subset->AttribId = currsubset;
						 subset->Enabled = (numwritten > 0);
						 subset->IndexCount = numwritten;
						 subset->IndexStart = indexoffset;
						 subset->PrimitiveType = GL_TRIANGLES;
						 subset->VertexCount = 0;
						 subset->VertexStart = 0;

						 indexoffset += numwritten;
						 idata += numwritten;

						 ++currsubset;
					 }
				 }
			 }
		 }
	 }

	 //OpenGLAttributeRange& lastsubset = (*subsettable)[currsubset - 1];
	 //printf("Total indices: %lu (%lu restarts)\n", lastsubset.IndexStart + lastsubset.IndexCount, numrestarts);
 }

void Vec2Normalize(glm::vec2& out, glm::vec2& v)
{
	float il = 1.0f / sqrtf(v.x * v.x + v.y * v.y);

	out[0] = v[0] * il;
	out[1] = v[1] * il;
}

//���������ȣ����滻��
float Vec2Length(const glm::vec2& v)
{
	return sqrtf(v.x * v.x + v.y * v.y);
}
//��log2x������
uint32_t Log2OfPow2(uint32_t x)
{
	uint32_t ret = 0;

	while (x >>= 1)
		++ret;

	return ret;
}
//����LOD������
static GLuint CalcSubsetIndex(int level, int dL, int dR, int dB, int dT)
{
	// returns the subset index of the given LOD levels
	return 2 * (level * 3 * 3 * 3 * 3 + dL * 3 * 3 * 3 + dR * 3 * 3 + dB * 3 + dT);
}