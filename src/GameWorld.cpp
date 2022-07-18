#include "GameWorld.h"
#include "Renderer.h"

using namespace DirectX;

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
		game->OnFocusChanged(inState);
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

	XMFLOAT3 xVec(1.0f, 0.0f, 0.0f);
	Logln("xVec: ", std::to_string(xVec.x), ", ", std::to_string(xVec.y), ", ", std::to_string(xVec.z));
	XMFLOAT3 yVec(0.0f, 1.0f, 0.0f);
	Logln("yVec: ", std::to_string(yVec.x), ", ", std::to_string(yVec.y), ", ", std::to_string(yVec.z));
	XMFLOAT3 zVec(0.0f, 0.0f, 1.0f);
	Logln("zVec: ", std::to_string(zVec.x), ", ", std::to_string(zVec.y), ", ", std::to_string(zVec.z));

	XMVECTOR rotVec = XMQuaternionRotationAxis(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMConvertToRadians(90.0f));

	XMMATRIX rotXMat = XMMatrixAffineTransformation(
		XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f), 
		XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), 
		rotVec,
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f)
	);
	XMVECTOR trXVec = XMVector4Transform(XMLoadFloat3(&xVec), rotXMat);
	XMFLOAT3 rotX;
	XMStoreFloat3(&rotX, trXVec);
	Logln("rotX: ", std::to_string(rotX.x), ", ", std::to_string(rotX.y), ", ", std::to_string(rotX.z));
	XMMATRIX rotYMat = XMMatrixAffineTransformation(
		XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f),
		XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		rotVec,
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f)
	);
	XMVECTOR trYVec = XMVector4Transform(XMLoadFloat3(&yVec), rotYMat);
	XMFLOAT3 rotY;
	XMStoreFloat3(&rotY, trYVec);
	Logln("rotY: ", std::to_string(rotY.x), ", ", std::to_string(rotY.y), ", ", std::to_string(rotY.z));
	XMMATRIX rotZMat = XMMatrixAffineTransformation(
		XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f),
		XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
		rotVec,
		XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f)
	);
	XMVECTOR trZVec = XMVector4Transform(XMLoadFloat3(&zVec), rotZMat);
	XMFLOAT3 rotZ;
	XMStoreFloat3(&rotZ, trZVec);
	Logln("rotZ: ", std::to_string(rotZ.x), ", ", std::to_string(rotZ.y), ", ", std::to_string(rotZ.z));

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

void GameWorld::OnFocusChanged(int inFocused) {
	bFocused = inFocused == GLFW_TRUE ? true : false;
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
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/plane.obj", "./../../../../Assets/Textures/slaataker1.png", "slaataker1", RenderTypes::EBlend));
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/plane.obj", "./../../../../Assets/Textures/slaataker2.png", "slaataker2", RenderTypes::EBlend));
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/plane.obj", "./../../../../Assets/Textures/slaataker3.png", "slaataker3", RenderTypes::EBlend));
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/viking_room.obj", "./../../../../Assets/Textures/viking_room.png", "viking1", RenderTypes::EOpaque, true));
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/viking_room.obj", "./../../../../Assets/Textures/viking_room.png", "viking2", RenderTypes::EOpaque, true));
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/viking_room.obj", "./../../../../Assets/Textures/viking_room.png", "viking3", RenderTypes::EOpaque, true));
	CheckReturn(mRenderer.AddModel("./../../../../Assets/Models/viking_room.obj", "./../../../../Assets/Textures/viking_room.png", "viking4", RenderTypes::EOpaque, true));

	return true;
}

void GameWorld::OnUnloadingData() {

}

bool GameWorld::GameLoop() {
	mTimer.Reset();

	while (!(glfwWindowShouldClose(mGLFWWindow) || bQuit)) {
		glfwPollEvents();

		mTimer.Tick();

		if (bFocused) CheckReturn(ProcessInput());
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

	auto cameraTarget = mCameraPos + glm::rotateY(glm::rotateX(ForwardVector, glm::radians(mPitch * -1.0f)), glm::radians(mYaw));
	mRenderer.UpdateCamera(mCameraPos, cameraTarget);

	auto correctQuat = glm::angleAxis(glm::radians(-90.0f), RightVector);

	auto slaatakerPos1 = glm::vec3(0.0f, 1.9f, -3.0f);
	auto slaatakerPos2 = glm::vec3(3.0f, 1.0f, -0.5f);
	auto slaatakerPos3 = glm::vec3(1.3f, 1.6f, 2.0f);

	auto dir1 = mCameraPos - slaatakerPos1;
	dir1.y = 0.0f;
	dir1 = glm::normalize(dir1);

	auto dir2 = mCameraPos - slaatakerPos2;
	dir2.y = 0.0f;
	dir2 = glm::normalize(dir2);

	auto dir3 = mCameraPos - slaatakerPos3;
	dir3.y = 0.0f;
	dir3 = glm::normalize(dir3);

	float dot1 = glm::dot(dir1, ForwardVector);
	float arcCos1 = std::acos(dot1);
	auto cross1 = glm::cross(ForwardVector, dir1);
	if (cross1.y < 0.0f) arcCos1 *= -1.0f;

	float dot2 = glm::dot(dir2, ForwardVector);
	float arcCos2 = std::acos(dot2);
	auto cross2 = glm::cross(ForwardVector, dir2);
	if (cross2.y < 0.0f) arcCos2 *= -1.0f;

	float dot3 = glm::dot(dir3, ForwardVector);
	float arcCos3 = std::acos(dot3);
	auto cross3 = glm::cross(ForwardVector, dir3);
	if (cross3.y < 0.0f) arcCos3 *= -1.0f;

	mRenderer.UpdateModel(
		"slaataker1", RenderTypes::EBlend,
		glm::vec3(1.0f),
		glm::angleAxis(glm::radians(180.0f) + arcCos1, UpVector) *
		correctQuat,
		slaatakerPos1
	);
	mRenderer.UpdateModel(
		"slaataker2", 
		RenderTypes::EBlend, 
		glm::vec3(1.0f), 
		glm::angleAxis(glm::radians(180.0f) + arcCos2, UpVector) *
		correctQuat,
		slaatakerPos2
	);
	mRenderer.UpdateModel(
		"slaataker3", 
		RenderTypes::EBlend, 
		glm::vec3(1.0f), 
		glm::angleAxis(glm::radians(180.0f) + arcCos3, UpVector) *
		correctQuat,
		slaatakerPos3
	);
	mRenderer.UpdateModel("viking1", 
		RenderTypes::EOpaque, 
		glm::vec3(6.0f), 
		correctQuat,
		glm::vec3(0.0f, 0.0f, 0.0f)
	);	
	mRenderer.UpdateModel("viking2",
		RenderTypes::EOpaque,
		glm::vec3(6.0f),
		glm::angleAxis(glm::radians(-90.0f), UpVector) *
		correctQuat,
		glm::vec3(0.0f, 0.0f, -8.9f)
	);
	mRenderer.UpdateModel("viking3",
		RenderTypes::EOpaque,
		glm::vec3(6.0f),
		glm::angleAxis(glm::radians(90.0f), UpVector) *
		correctQuat,
		glm::vec3(8.9f, 0.0f, 0.0f)
	);
	mRenderer.UpdateModel("viking4",
		RenderTypes::EOpaque,
		glm::vec3(6.0f),
		glm::angleAxis(glm::radians(180.0f), UpVector) *
		correctQuat,
		glm::vec3(8.9f, 0.0f, -8.9f)
	);

	CheckReturn(mRenderer.Update(gt));

	return true;
}

bool GameWorld::Draw() {
	CheckReturn(mRenderer.Draw());

	return true;
}