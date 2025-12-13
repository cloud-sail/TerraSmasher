#include "Game/GameCommon.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Window/Window.hpp"

float g_screenWidth = 1600.0f;
float g_screenHeight = 800.0f;
unsigned int g_windowWidth = 1600;
unsigned int g_windowHeight = 800;

void UpdateScreenDimensions()
{
	IntVec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();

	g_windowWidth = static_cast<unsigned int>(clientDimensions.x);
	g_windowHeight = static_cast<unsigned int>(clientDimensions.y);

	g_screenWidth = static_cast<float>(clientDimensions.x);
	g_screenHeight = static_cast<float>(clientDimensions.y);
}
