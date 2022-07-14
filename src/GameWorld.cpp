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

	void GLFWWindowPosCallback(GLFWwindow* pWnd, int inX, int inY) {
		auto game = reinterpret_cast<GameWorld*>(glfwGetWindowUserPointer(pWnd));
		game->OnPositionChanged(inX, inY);
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
	CheckReturn(OnLoadingData());
	CheckReturn(GameLoop());
	OnUnloadingData();

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

void GameWorld::OnPositionChanged(int inX, int inY) {
	mClientPosX = inX;
	mClientPosY = inY;
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
	glfwSetWindowPosCallback(mGLFWWindow, GLFWWindowPosCallback);

	glfwSetInputMode(mGLFWWindow, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	return true;
}

bool GameWorld::OnLoadingData() {
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/plane.obj", "slaataker1"));
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/plane.obj", "slaataker2"));
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/plane.obj", "slaataker3"));
	CheckReturn(mRenderer.AddTexture("./../../../../Assets/Textures/slaataker2.png"));

	return true;
}

void GameWorld::OnUnloadingData() {

}

bool GameWorld::GameLoop() {
	mTimer.Reset();

	while (!(glfwWindowShouldClose(mGLFWWindow) || bQuit)) {
		glfwPollEvents();

		mTimer.Tick();

		CheckReturn(ProcessInput());
		CheckReturn(Update(mTimer));
		CheckReturn(Draw());

		Sleep(10);
	}

	return true;
}

bool GameWorld::ProcessInput() {
	mForward = 0.0f;
	mStrape = 0.0f;
	
	int upState = glfwGetKey(mGLFWWindow, GLFW_KEY_W);
	if (upState == GLFW_PRESS) {
		mForward += 1.0f;
	}

	int downState = glfwGetKey(mGLFWWindow, GLFW_KEY_S);
	if (downState == GLFW_PRESS) {
		mForward += -1.0f;
	}

	int leftState = glfwGetKey(mGLFWWindow, GLFW_KEY_A);
	if (leftState == GLFW_PRESS) {
		mStrape += -1.0f;
	}

	int rightState = glfwGetKey(mGLFWWindow, GLFW_KEY_D);
	if (rightState == GLFW_PRESS) {
		mStrape += 1.0f;
	}

	double xpos = 0;
	double ypos = 0;
	glfwGetCursorPos(mGLFWWindow, &xpos, &ypos);

	double centerXPos = mClientPosX + mClientWidth * 0.5f;
	double centerYPos = mClientPosY + mClientHeight * 0.5f;

	mCursorDeltaX = xpos - centerXPos;
	mCursorDeltaY = ypos - centerYPos;

	glfwSetCursorPos(mGLFWWindow, centerXPos, centerYPos);

	return true;
}

bool GameWorld::Update(const GameTimer& gt) {
	glm::vec3 strape = RightVector * mStrape;
	glm::vec3 forward = ForwardVector * mForward;
	glm::vec3 disp = strape + forward;

	auto worldDisp = glm::rotateY(disp, glm::radians(mYaw));
	mCameraPos += worldDisp * gt.DeltaTime();

	mYaw += static_cast<float>(mCursorDeltaX) * 4.0f * gt.DeltaTime();
	mPitch = std::min(std::max(-45.0f, mPitch + static_cast<float>(mCursorDeltaY) * -4.0f * gt.DeltaTime()), 45.0f);

	auto cameraTarget = mCameraPos + glm::rotateX(glm::rotateY(ForwardVector, glm::radians(mYaw)), glm::radians(mPitch));
	mRenderer.UpdateCamera(mCameraPos, cameraTarget);

	mRenderer.UpdateModel("slaataker1", glm::vec3(1.0f), glm::vec3(1.0f, 0.0f, -1.0f));
	mRenderer.UpdateModel("slaataker2", glm::vec3(1.0f), glm::vec3(-0.5f, 0.0f, 0.0f));
	mRenderer.UpdateModel("slaataker3", glm::vec3(1.0f), glm::vec3(0.0f, 1.0f, -2.0f));

	CheckReturn(mRenderer.Update(gt));

	return true;
}

bool GameWorld::Draw() {
	CheckReturn(mRenderer.Draw());

	return true;
}