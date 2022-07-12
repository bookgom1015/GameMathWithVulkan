#include "GameWorld.h"
#include "Renderer.h"

namespace {
	void GLFWKeyCallback(GLFWwindow* pWnd, int inKey, int inScanCode, int inAction, int inMods) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(pWnd));
		game->MsgProc(pWnd, inKey, inScanCode, inAction, inMods);
	}

	void GLFWResizeCallback(GLFWwindow* pWnd, int inWidth, int inHeight) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(pWnd));
		game->OnResize(inWidth, inHeight);
	}

	void GLFWFocusCallback(GLFWwindow* pWnd, int inState) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(pWnd));
		
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd) {
	GameWorld game;

	CheckReturn(game.Initialize());
	CheckReturn(game.RunLoop());
	game.CleanUp();

	return 0;
}

GameWorld::~GameWorld() {
	if (!bIsCleanedUp) {
		CleanUp();
	}
}

bool GameWorld::Initialize() {
	CheckReturn(InitMainWnd());
	CheckReturn(mRenderer.Initialize(mClientWidth, mClientHeight, mGLFWWindow));

	return true;
}

bool GameWorld::RunLoop() {
	GameLoop();

	return true;
}

void GameWorld::CleanUp() {
	mRenderer.CleanUp();

	glfwDestroyWindow(mGLFWWindow);
	glfwTerminate();

	bIsCleanedUp = true;
}

void GameWorld::MsgProc(GLFWwindow* pWnd, int inKey, int inScanCode, int inAction, int inMods) {
	switch (inKey) {
	case GLFW_KEY_ESCAPE:
		if (inAction == GLFW_PRESS) {
			bQuit = true;
		}
		return;
	default:
		return;
	}
}

void GameWorld::OnResize(int inWidth, int inHeight) {
	mRenderer.OnResize(inWidth, inHeight);
}

bool GameWorld::InitMainWnd() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	mGLFWWindow = glfwCreateWindow(mClientWidth, mClientHeight, "GameMath", nullptr, nullptr);
	mhMainWnd = glfwGetWin32Window(mGLFWWindow);

	auto primaryMonitor = glfwGetPrimaryMonitor();
	const auto videoMode = glfwGetVideoMode(primaryMonitor);

	int clientPosX = (videoMode->width - mClientWidth) >> 1;
	int clientPosY = (videoMode->height - mClientHeight) >> 1;

	glfwSetWindowPos(mGLFWWindow, clientPosX, clientPosY);

	glfwSetWindowUserPointer(mGLFWWindow, this);

	glfwSetKeyCallback(mGLFWWindow, GLFWKeyCallback);
	glfwSetFramebufferSizeCallback(mGLFWWindow, GLFWResizeCallback);
	glfwSetWindowFocusCallback(mGLFWWindow, GLFWFocusCallback);

	return true;
}

bool GameWorld::OnLoadingData() {
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/plane.obj"));

	return true;
}

void GameWorld::OnUnloadingData() {

}

bool GameWorld::GameLoop() {
	CheckReturn(OnLoadingData());

	mTimer.Reset();

	while (!(glfwWindowShouldClose(mGLFWWindow) || bQuit)) {
		glfwPollEvents();

		mTimer.Tick();

		mRenderer.Update(mTimer);
		mRenderer.Draw();

		Sleep(10);
	}

	OnUnloadingData();

	return true;
}