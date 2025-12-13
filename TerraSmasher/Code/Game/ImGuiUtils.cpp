#include "Game/ImGuiUtils.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "ThirdParty/imgui/imgui.h"


#include <filesystem>
#include <numeric>
namespace fs = std::filesystem;

void DrawVec3Control(const std::string& label, Vec3& values, float resetValue, float columnWidth)
{
	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	ImGui::Text("%s", label.c_str());
	ImGui::NextColumn();

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

	float width = ImGui::CalcItemWidth();
	float lineHeight = ImGui::GetFrameHeight();
	ImVec2 buttonSize = { lineHeight, lineHeight };

	// X
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
		values.x = resetValue;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	ImGui::SetNextItemWidth((width - buttonSize.x * 3) / 3.0f);
	ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::SameLine();

	// Y
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
		values.y = resetValue;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	ImGui::SetNextItemWidth((width - buttonSize.x * 3) / 3.0f);
	ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
	ImGui::SameLine();

	// Z
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	if (ImGui::Button("Z", buttonSize))
		values.z = resetValue;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	ImGui::SetNextItemWidth((width - buttonSize.x * 3) / 3.0f);
	ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");

	ImGui::PopStyleVar();
	ImGui::Columns(1);
	ImGui::PopID();
}

void DrawEulerAnglesControl(const std::string& label, EulerAngles& values, float resetValue /*= 0.0f*/, float columnWidth /*= 100.0f*/)
{
	constexpr float DRAG_SPEED = 0.3f;

	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	ImGui::Text("%s", label.c_str());
	ImGui::NextColumn();

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

	float width = ImGui::CalcItemWidth();
	float lineHeight = ImGui::GetFrameHeight();
	ImVec2 buttonSize = { lineHeight, lineHeight };

	// X
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
	if (ImGui::Button("X", buttonSize))
		values.m_rollDegrees = resetValue;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	ImGui::SetNextItemWidth((width - buttonSize.x * 3) / 3.0f);
	ImGui::DragFloat("##X", &values.m_rollDegrees, DRAG_SPEED, 0.0f, 0.0f, "%.2f");
	ImGui::SameLine();

	// Y
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
	if (ImGui::Button("Y", buttonSize))
		values.m_pitchDegrees = resetValue;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	ImGui::SetNextItemWidth((width - buttonSize.x * 3) / 3.0f);
	ImGui::DragFloat("##Y", &values.m_pitchDegrees, DRAG_SPEED, 0.0f, 0.0f, "%.2f");
	ImGui::SameLine();

	// Z
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
	if (ImGui::Button("Z", buttonSize))
		values.m_yawDegrees = resetValue;
	ImGui::PopStyleColor(3);
	ImGui::SameLine();
	ImGui::SetNextItemWidth((width - buttonSize.x * 3) / 3.0f);
	ImGui::DragFloat("##Z", &values.m_yawDegrees, DRAG_SPEED, 0.0f, 0.0f, "%.2f");

	values.Normalize();


	ImGui::PopStyleVar();
	ImGui::Columns(1);
	ImGui::PopID();
}

void DrawFloatControl(const std::string& label, float& value, float resetValue /*= 0.0f*/, float columnWidth /*= 100.0f*/)
{
	ImGui::PushID(label.c_str());

	ImGui::Columns(2);
	ImGui::SetColumnWidth(0, columnWidth);
	ImGui::Text("%s", label.c_str());
	ImGui::NextColumn();

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

	float width = ImGui::CalcItemWidth();

	float lineHeight = ImGui::GetFrameHeight();
	ImVec2 buttonSize = { lineHeight, lineHeight };

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.6f, 0.6f, 0.6f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });
	if (ImGui::Button("R", buttonSize))
		value = resetValue;
	ImGui::PopStyleColor(3);

	ImGui::SameLine();
	ImGui::SetNextItemWidth((width - buttonSize.x));
	ImGui::DragFloat("##Value", &value, 0.1f, 0.0f, 0.0f, "%.2f");

	ImGui::PopStyleVar();
	ImGui::Columns(1);
	ImGui::PopID();
}

GameFileSelector::GameFileSelector(std::string const& directory, std::string const& extension)
	: m_directory(directory)
	, m_extension(extension)
	, m_selectedIndex(-1)
{
	ScanDirectory();
}

void GameFileSelector::Render()
{
	ImGui::Text("Directory: %s", m_directory.c_str());
	ImGui::Text("Extension: %s", m_extension.c_str());
	ImGui::Separator();

	if (m_fileNames.empty()) 
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No files found");
		return;
	}

	ImGui::Text("Files (%zu):", m_fileNames.size());

	if (ImGui::BeginListBox("##files", ImVec2(-FLT_MIN, 15 * ImGui::GetTextLineHeightWithSpacing()))) 
	{
		for (int i = 0; i < static_cast<int>(m_fileNames.size()); ++i) 
		{
			const bool isSelected = (m_selectedIndex == i);

			if (ImGui::Selectable(m_fileNames[i].c_str(), isSelected)) 
			{
				m_selectedIndex = i;
			}

			if (isSelected) 
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndListBox();
	}

	if (HasSelection()) 
	{
		ImGui::Separator();
		ImGui::Text("Selected: %s", m_fileNames[m_selectedIndex].c_str());
		ImGui::TextWrapped("Path: %s", m_filePaths[m_selectedIndex].c_str());
	}
}

std::optional<std::string> GameFileSelector::GetSelectedFilePath() const
{
	if (HasSelection()) 
	{
		return m_filePaths[m_selectedIndex];
	}
	return std::nullopt;
}

bool GameFileSelector::HasSelection() const
{
	return m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_filePaths.size());
}

void GameFileSelector::Refresh()
{
	ScanDirectory();
}

void GameFileSelector::SetDirectory(std::string const& directory)
{
	m_directory = directory;
	ScanDirectory();
}

void GameFileSelector::SetExtension(std::string const& extension)
{
	m_extension = extension;
	ScanDirectory();
}

void GameFileSelector::ScanDirectory()
{
	m_fileNames.clear();
	m_filePaths.clear();
	m_selectedIndex = -1;

	if (!fs::exists(m_directory) || !fs::is_directory(m_directory))
	{
		return;
	}

	for (auto const& entry : fs::directory_iterator(m_directory))
	{
		if (entry.is_regular_file() && entry.path().extension() == m_extension)
		{
			m_fileNames.push_back(entry.path().filename().string());
			m_filePaths.push_back(entry.path().string());
		}
	}

	std::vector<size_t> indices(m_fileNames.size());
	std::iota(indices.begin(), indices.end(), 0);

	std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
		return m_fileNames[a] < m_fileNames[b];
		});

	std::vector<std::string> sortedNames;
	std::vector<std::string> sortedPaths;

	for (size_t idx : indices) 
	{
		sortedNames.push_back(m_fileNames[idx]);
		sortedPaths.push_back(m_filePaths[idx]);
	}

	m_fileNames = std::move(sortedNames);
	m_filePaths = std::move(sortedPaths);
}
