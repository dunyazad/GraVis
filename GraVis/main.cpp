#include <stdio.h>
#include <locale.h>
#include <math.h>

#include <winsock2.h> // 윈속 헤더 포함 
#include <windows.h> 
#pragma comment (lib,"ws2_32.lib") // 윈속 라이브러리 링크
#define BUFFER_SIZE 1024 // 버퍼 사이즈

#include <vector>
#include <thread>
using namespace std;

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifndef _DEBUG
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "glew32.lib")
#else
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glfw3d.lib")
#pragma comment(lib, "glew32.lib")
#endif



#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "implot/implot.h"

int windowWidth = 1200;
int windowHeight = 800;

// utility structure for realtime plot
struct ScrollingBuffer {
	int MaxSize;
	int Offset;
	ImVector<ImVec2> Data;
	ScrollingBuffer() {
		MaxSize = 2000;
		Offset = 0;
		Data.reserve(MaxSize);
	}
	void AddPoint(float x, float y) {
		if (Data.size() < MaxSize)
			Data.push_back(ImVec2(x, y));
		else {
			Data[Offset] = ImVec2(x, y);
			Offset = (Offset + 1) % MaxSize;
		}
	}
	void Erase() {
		if (Data.size() > 0) {
			Data.shrink(0);
			Offset = 0;
		}
	}
};

// utility structure for realtime plot
struct RollingBuffer {
	float Span;
	ImVector<ImVec2> Data;
	RollingBuffer() {
		Span = 10.0f;
		Data.reserve(2000);
	}
	void AddPoint(float x, float y) {
		float xmod = fmodf(x, Span);
		if (!Data.empty() && xmod < Data.back().x)
			Data.shrink(0);
		Data.push_back(ImVec2(xmod, y));
	}
};

ScrollingBuffer sdata1;
RollingBuffer   rdata1;

vector<float> times;
vector<float> datas;

void resize_callback(GLFWwindow* window, int width, int height);

int main(void)
{
	_wsetlocale(LC_ALL, L"korean");

	GLFWwindow* window;


	/* Initialize the library */
	if (!glfwInit())
		return -1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	//glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	/* Create a windowed mode window and its OpenGL context */
	window = glfwCreateWindow(windowWidth, windowHeight, "GraVis", NULL, NULL);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}

	/* Make the window's context current */
	glfwMakeContextCurrent(window);



	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		printf("Error: %s\n", glewGetErrorString(err));
	}
	printf("Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));


	glfwSetWindowSizeCallback(window, resize_callback);




	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer backends
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 330");

		// Load Fonts
		// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
		// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
		// - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
		// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
		// - Read 'docs/FONTS.md' for more instructions and details.
		// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
		//io.Fonts->AddFontDefault();
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
		//io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
		//ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
		//IM_ASSERT(font != NULL);
	}

	ImPlot::CreateContext();


	bool show_demo_window = true;
	bool show_another_window = false;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);






	WSADATA   wsaData;
	SOCKADDR_IN  sockAddr;
	SOCKET   receivingSocket;
	thread receivingThread;
	bool bRunning = true;

	{
		if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR)
		{
			WSACleanup();
		}

		memset(&sockAddr, 0, sizeof(sockAddr));

		sockAddr.sin_family = AF_INET;
		sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		sockAddr.sin_port = htons(12120);

		receivingSocket = socket(AF_INET, SOCK_DGRAM, 0);
		if (receivingSocket == INVALID_SOCKET)
		{
			closesocket(receivingSocket);
			WSACleanup();
		}

		bind(receivingSocket, (SOCKADDR*)&sockAddr, sizeof(sockAddr));

		//int errCode = WSAGetLastError();
		//printf("ErrorCode : %d\n", errCode);

		receivingThread = thread([&]() {
			while (bRunning)
			{
				char buffer[2048];
				memset(buffer, 0, 2048);
				int fromServerSize = sizeof(sockAddr);
				int result = recvfrom(receivingSocket, buffer, 2048, 0, (struct sockaddr*)&sockAddr, &fromServerSize);

				if (result == SOCKET_ERROR)
				{
					int errCode = WSAGetLastError();
					printf("ErrorCode : %d\n", errCode);
				}

				float t = 0;
				memcpy(&t, buffer, sizeof(float));
				float d = 0;
				memcpy(&d, buffer + sizeof(float), sizeof(float));
				
				sdata1.AddPoint(t, d);
				rdata1.AddPoint(t, d);

				times.push_back(t);
				datas.push_back(d);

				printf("%f\n", d);
			}
			});
	}



	/*  dataXs.Add(0);
	  dataYs.push_back(0);*/

	sdata1.AddPoint(0, 0);
	rdata1.AddPoint(0, 0);

	  /* Loop until the user closes the window */
	while (!glfwWindowShouldClose(window))
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		//ImPlot::ShowDemoWindow(&show_demo_window);

		//ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once | ImGuiCond_Appearing);
		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

		ImGui::Begin("Test", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

		static ImPlotAxisFlags rt_axis = ImPlotAxisFlags_NoTickLabels;

		if (times.size() != 0)
		{
			if (ImPlot::BeginPlot("##Scrolling", NULL, NULL, ImVec2(-1, -1), 0, rt_axis, rt_axis)) {
				ImPlot::PlotLine("value", &times[0], &datas[0], times.size(), times.size());
				ImPlot::EndPlot();
			}
		}

		ImGui::End();

		//double DEMO_TIME = ImGui::GetTime();

		// Rendering
		ImGui::Render();


		/* Render here */
		glClear(GL_COLOR_BUFFER_BIT);



		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


		/* Swap front and back buffers */
		glfwSwapBuffers(window);

		/* Poll for and process events */
		glfwPollEvents();
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	glfwTerminate();

	bRunning = false;
	closesocket(receivingSocket);
	WSACleanup();
	receivingThread.join();

	return 0;
}

void resize_callback(GLFWwindow* window, int width, int height)
{
	windowWidth = width;
	windowHeight = height;
}
