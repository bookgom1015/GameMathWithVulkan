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

private:
	bool InitMainWnd();

	bool OnLoadingData();
	void OnUnloadingData();

	bool GameLoop();

private:
	bool bQuit = false;
	bool bIsCleanedUp = false;

	int mClientWidth = 800;
	int mClientHeight = 600;

	GLFWwindow* mGLFWWindow;
	HANDLE mhMainWnd;

	Renderer mRenderer;
	GameTimer mTimer;
};