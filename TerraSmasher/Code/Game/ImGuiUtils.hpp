#pragma once
#include <string>
#include <vector>
#include <optional>



struct Vec3;
struct EulerAngles;

void DrawVec3Control(const std::string& label, Vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f);
void DrawEulerAnglesControl(const std::string& label, EulerAngles& values, float resetValue = 0.0f, float columnWidth = 100.0f);
void DrawFloatControl(const std::string& label, float& value, float resetValue = 0.0f, float columnWidth = 100.0f);


class GameFileSelector
{
public:
	GameFileSelector(std::string const& directory, std::string const& extension);

	void Render();

	std::optional<std::string> GetSelectedFilePath() const;

	bool HasSelection() const;

	void Refresh();

	void SetDirectory(std::string const& directory);

	void SetExtension(std::string const& extension);

private:
	void ScanDirectory();
	
	std::string m_directory;
	std::string m_extension;
	std::vector<std::string> m_fileNames;
	std::vector<std::string> m_filePaths;
	int m_selectedIndex;
};

//GameFileSelector m_testFileSelector;
//: m_testFileSelector("Data\\Shaders", ".hlsl")


//m_testFileSelector.Render();
//ImGui::BeginDisabled(!m_testFileSelector.HasSelection());
//if (ImGui::Button("Print Current Selection"))
//{
//	auto filePath = m_testFileSelector.GetSelectedFilePath();
//	if (filePath.has_value())
//	{
//		DebugAddMessage(filePath.value(), 3.f);
//	}
//}
//ImGui::EndDisabled();
