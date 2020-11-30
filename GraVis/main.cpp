#include <stdio.h>
#include <locale.h>
#include <math.h>

#include <winsock2.h> // 윈속 헤더 포함 
#include <windows.h> 
#pragma comment (lib,"ws2_32.lib") // 윈속 라이브러리 링크
#define BUFFER_SIZE 1024 // 버퍼 사이즈

#include <map>
#include <string>
#include <thread>
#include <vector>
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

//// utility structure for realtime plot
//struct ScrollingBuffer {
//	int MaxSize;
//	int Offset;
//	ImVector<ImVec2> Data;
//	ScrollingBuffer() {
//		MaxSize = 2000;
//		Offset = 0;
//		Data.reserve(MaxSize);
//	}
//	void AddPoint(float x, float y) {
//		if (Data.size() < MaxSize)
//			Data.push_back(ImVec2(x, y));
//		else {
//			Data[Offset] = ImVec2(x, y);
//			Offset = (Offset + 1) % MaxSize;
//		}
//	}
//	void Erase() {
//		if (Data.size() > 0) {
//			Data.shrink(0);
//			Offset = 0;
//		}
//	}
//};

struct RollingBuffer {
	float Span;
	ImVector<ImVec2> Data;
	float yMin = FLT_MAX;
	float yMax = FLT_MIN;
	RollingBuffer() {
		Span = 10.0f;
		Data.reserve(2000);
	}
	void AddPoint(float x, float y) {
		float xmod = fmodf(x, Span);
		if (!Data.empty() && xmod < Data.back().x)
		{
			Data.shrink(0);

			yMin = FLT_MAX;
			yMax = FLT_MIN;
		}
		Data.push_back(ImVec2(xmod, y));

		yMin = yMin < y ? yMin : y;
		yMax = yMax > y ? yMax : y;
	}
};

map<string, RollingBuffer> buffers;
bool bPaused = false;

//ScrollingBuffer sdata1;
//RollingBuffer   rdata1;

void resize_callback(GLFWwindow* window, int width, int height);

void DrawBuffer(RollingBuffer& buffer, string label, float history);

int main(void)
{
	_wsetlocale(LC_ALL, L"korean");

	GLFWwindow* window;

#pragma region Initialize GLFW
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

	glfwSetWindowSizeCallback(window, resize_callback);
#pragma endregion

#pragma region Initialize GLEW
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		printf("Error: %s\n", glewGetErrorString(err));
	}
	printf("Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
#pragma endregion

#pragma region Network
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

				if (bPaused == false)
				{
					int index = 0;
					float t = 0;
					memcpy(&t, buffer + index, sizeof(float)); index += sizeof(float);

					{
						float pitch = 0;
						memcpy(&pitch, buffer + index, sizeof(float)); index += sizeof(float);
						float yaw = 0;
						memcpy(&yaw, buffer + index, sizeof(float)); index += sizeof(float);
						float roll = 0;
						memcpy(&roll, buffer + index, sizeof(float)); index += sizeof(float);

						buffers["EulerAngles.pitch"].AddPoint(t, pitch);
						buffers["EulerAngles.yaw"].AddPoint(t, yaw);
						buffers["EulerAngles.roll"].AddPoint(t, roll);
					}
					{
						float pitch = 0;
						memcpy(&pitch, buffer + index, sizeof(float)); index += sizeof(float);
						float yaw = 0;
						memcpy(&yaw, buffer + index, sizeof(float)); index += sizeof(float);
						float roll = 0;
						memcpy(&roll, buffer + index, sizeof(float)); index += sizeof(float);

						buffers["AngularVelocity.pitch"].AddPoint(t, pitch);
						buffers["AngularVelocity.yaw"].AddPoint(t, yaw);
						buffers["AngularVelocity.roll"].AddPoint(t, roll);
					}
					{
						float pitch = 0;
						memcpy(&pitch, buffer + index, sizeof(float)); index += sizeof(float);
						float yaw = 0;
						memcpy(&yaw, buffer + index, sizeof(float)); index += sizeof(float);
						float roll = 0;
						memcpy(&roll, buffer + index, sizeof(float)); index += sizeof(float);

						buffers["AngularAcceleration.pitch"].AddPoint(t, pitch);
						buffers["AngularAcceleration.yaw"].AddPoint(t, yaw);
						buffers["AngularAcceleration.roll"].AddPoint(t, roll);
					}
					{
						float pitch = 0;
						memcpy(&pitch, buffer + index, sizeof(float)); index += sizeof(float);
						float yaw = 0;
						memcpy(&yaw, buffer + index, sizeof(float)); index += sizeof(float);
						float roll = 0;
						memcpy(&roll, buffer + index, sizeof(float)); index += sizeof(float);

						buffers["PredictionError.pitch"].AddPoint(t, pitch);
						buffers["PredictionError.yaw"].AddPoint(t, yaw);
						buffers["PredictionError.roll"].AddPoint(t, roll);
					}
				}
			}
			});
	}
#pragma endregion

#pragma region Initialize IMPLOT
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
#pragma endregion

	buffers["EulerAngles.pitch"];
	buffers["EulerAngles.yaw"];
	buffers["EulerAngles.roll"];

	buffers["AngularVelocity.pitch"];
	buffers["AngularVelocity.yaw"];
	buffers["AngularVelocity.roll"];

	buffers["AngularAcceleration.pitch"];
	buffers["AngularAcceleration.yaw"];
	buffers["AngularAcceleration.roll"];

	buffers["PredictionError.pitch"];
	buffers["PredictionError.yaw"];
	buffers["PredictionError.roll"];

	while (!glfwWindowShouldClose(window))
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		//ImPlot::ShowDemoWindow(&show_demo_window);

		ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight));

		ImGui::Begin("Test", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
		{
			ImGui::Checkbox("Pause", &bPaused);

			static float history = 10.0f;
			ImGui::SliderFloat("History", &history, 1, 30, "%.1f s");

			static ImPlotAxisFlags rt_axis = ImPlotAxisFlags_NoTickLabels;

			if (ImGui::CollapsingHeader("Pitch", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen)) {
				DrawBuffer(buffers["EulerAngles.pitch"], "Euler Angles Pitch", history);
				DrawBuffer(buffers["AngularVelocity.pitch"], "Angular Velocity Pitch", history);
				DrawBuffer(buffers["AngularAcceleration.pitch"], "Angular Acceleration Pitch", history);
				DrawBuffer(buffers["PredictionError.pitch"], "PredictionError Pitch", history);
			}

			if (ImGui::CollapsingHeader("Yaw", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen)) {
				DrawBuffer(buffers["EulerAngles.yaw"], "Euler Angles Yaw", history);
				DrawBuffer(buffers["AngularVelocity.yaw"], "Angular Velocity Yaw", history);
				DrawBuffer(buffers["AngularAcceleration.yaw"], "Angular Acceleration Yaw", history);
				DrawBuffer(buffers["PredictionError.yaw"], "PredictionError Yaw", history);
			}

			if (ImGui::CollapsingHeader("Roll", ImGuiTreeNodeFlags_CollapsingHeader | ImGuiTreeNodeFlags_DefaultOpen)) {
				DrawBuffer(buffers["EulerAngles.roll"], "Euler Angles Roll", history);
				DrawBuffer(buffers["AngularAcceleration.roll"], "Angular Acceleration Roll", history);
				DrawBuffer(buffers["AngularVelocity.roll"], "Angular Velocity Roll", history);
				DrawBuffer(buffers["PredictionError.roll"], "PredictionError Roll", history);
			}
		}
		ImGui::End();

		ImGui::Render();

		glClear(GL_COLOR_BUFFER_BIT);

		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

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

void DrawBuffer(RollingBuffer& buffer, string label, float history)
{
	if (buffer.Data.Size != 0)
	{
		ImPlotAxisFlags rt_axis = ImPlotAxisFlags_NoTickLabels;
		buffer.Span = history;
		ImGui::LabelText(label.c_str(), " min : %.4f, Max : %.4f, current : %.4f", buffer.yMin, buffer.yMax, buffer.Data.back().y);
		ImPlot::SetNextPlotLimitsX(0, history, ImGuiCond_Always);
		ImPlot::SetNextPlotLimitsY(buffer.yMin, buffer.yMax, ImGuiCond_Always);
		if (ImPlot::BeginPlot(label.c_str(), NULL, NULL, ImVec2(-1, (windowHeight - 140) / 4), 0, rt_axis, rt_axis)) {
			ImPlot::PlotLine(label.c_str(), &buffer.Data[0].x, &buffer.Data[0].y, buffer.Data.size(), 0, 2 * sizeof(float));
			ImPlot::EndPlot();
		}
	}
}
