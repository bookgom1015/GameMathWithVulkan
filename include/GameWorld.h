#pragma once

#include "Renderer.h"

class GameWorld {
public:
	GameWorld() = default;
	virtual ~GameWorld();

private:
	GameWorld(const GameWorld& inRef) = delete;
	GameWorld(GameWorld&& inRVal) = delete;
	virtual GameWorld& operator=(const GameWorld& inRef) = delete;
	virtual GameWorld& operator=(GameWorld&& inRVal) = delete;

public:
	bool Initialize();
	bool RunLoop();
	void CleanUp();

	void MsgProc(GLFWwindow* pWnd, int inKey, int inScanCode, int inAction, int inMods);
	void OnResize(int inWidth, int inHeight);
	void OnPositionChanged(int inX, int inY);
	void OnFocusChanged(int inFocused);

protected:
	bool InitMainWnd();

	bool OnLoadingData();
	void OnUnloadingData();

	bool GameLoop();

	bool ProcessInput();
	bool Update(const GameTimer& gt);
	bool Draw();

private:
	bool bQuit = false;
	bool bFocused = true;
	bool bIsCleanedUp = false;

	int mClientWidth = 800;
	int mClientHeight = 600;
	int mClientPosX = 0;
	int mClientPosY = 0;

	GLFWwindow* mGLFWWindow;
	HANDLE mhMainWnd;

	Renderer mRenderer;
	GameTimer mTimer;

	float mForward = 0;
	float mStrape = 0;

	double mCursorDeltaX = 0.0f;
	double mCursorDeltaY = 0.0f;

	glm::vec3 mCameraPos = glm::vec3(0.0f, 2.0f, -6.0f);
	float mPitch = 0.0f;
	float mYaw = 0.0f;
};