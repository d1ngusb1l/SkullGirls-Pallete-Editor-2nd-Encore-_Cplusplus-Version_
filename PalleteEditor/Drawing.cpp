#include "Drawing.h"
#include "PalleteEditor.h"
#include "FileLoad.h"
#include "Auto-Load-Pallete.h"
#include "tinyfiledialogs.h"
#include "ColorWheel.h"
#include "Config.h"
#include "Data/AtomicFile.h"

void Drawing::Active()
{
	bDraw = true;
}

bool Drawing::isActive()
{
	return bDraw == true;
}

void Drawing::Draw()
{
	if (isActive())
	{
		static std::unordered_map<std::string, bool> wheelOpenMap;

		PalEdit::Init();
		ImGui::SetNextWindowSize(vWindowSize, ImGuiCond_Once);
		ImGui::SetNextWindowBgAlpha(1.0f);
		ImGui::Begin(lpWindowName, &bDraw, WindowFlags);
		{
			//Меню-Бар
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Load Table"))
					{
						AddressTable::LoadFromFile();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Load JSON (Characters parts)"))
					{
						GroupColorGroup::LoadFromFile();
					}
					if (PalEdit::bGameOpenned and PalEdit::bMatchStarted and (PalEdit::current_character_idx != -1)) {
						ImGui::Separator();
						if (ImGui::MenuItem("Save palette"))
						{
							PalleteFile::SaveToFile(
								PalEdit::Character_Vector[PalEdit::FindVectorIndexByID(PalEdit::current_character_idx)]
							);
						}
						if (ImGui::MenuItem("Save palette and auto-load")) {
                            auto& character=PalEdit::Character_Vector[PalEdit::FindVectorIndexByID(PalEdit::current_character_idx)];
                            if(PalleteFile::SaveToFile(character)) AutoPallete::Assign(character.Char_Name,character.Current_Pallete_Num,PaletteData::PathText(PalleteFile::LastSavedPath));
                        }
                        if (ImGui::MenuItem("Export legacy colors only")) {
                            PalleteFile::SaveToFile(PalEdit::Character_Vector[PalEdit::FindVectorIndexByID(PalEdit::current_character_idx)],true);
                        }
                        if (ImGui::MenuItem("Load palette")) {
                            PalleteFile::LoadFromFile(PalEdit::Character_Vector[PalEdit::FindVectorIndexByID(PalEdit::current_character_idx)]);
                        }
					}
					ImGui::EndMenu();
	
				}

				// Кнопка About прямо в меню баре
				if (ImGui::MenuItem("About"))
				{
					// Открываем окно About при нажатии
					bShow_about_window = true;
				}

				ImGui::EndMenuBar();
			}
				//Окно "About"
				if (bShow_about_window)
				{
					ImGui::Begin("About", &bShow_about_window);
					ImGui::Text("Skullgirls Palette Editor");
					ImGui::TextDisabled("Version: 1.0.0-rc.1");
					ImGui::Text("Also, check our Discord: ");
					ImGui::TextLinkOpenURL("Discord", "https://discord.gg/4ufGJQjkpc");
					ImGui::Separator();
					if (ImGui::Button("OK"))
					{
						bShow_about_window = false;
					}
					ImGui::End();
				}
				if (ImGui::BeginTabBar("##TabBar")) {
				if (ImGui::BeginTabItem("Palette")) {
                    if(!PalleteFile::Status.empty()) ImGui::TextWrapped("%s",PalleteFile::Status.c_str());
                    if(!config::Status.empty()) ImGui::TextWrapped("%s",config::Status.c_str());
			
				if (PalEdit::bGameOpenned != true) {
					ImGui::Text("Open the game to use the editor.");

				}
				else {
					if (PalEdit::bMatchStarted != true) {
						ImGui::Text("Start a match to use the editor.\nIf this message persists, the table may be invalid.");
					}
					else {
						//Комбо-бокс выбора персонажа
						ImGui::Text("Select character:");
						const char* preview_text = "Select";
						for (const auto& character : PalEdit::Character_Vector) {
							if (character.ID == PalEdit::current_character_idx) {
								preview_text = character.Char_Name.c_str();
								break;
							}
						}

						if (ImGui::BeginCombo("##CharSelect", preview_text)) {
							for (int i = 0; i < PalEdit::Character_Vector.size(); i++) {
								// Сравниваем ID, а не индекс в векторе
								bool is_selected = (PalEdit::current_character_idx == PalEdit::Character_Vector[i].ID);

								std::string Display_Name = PalEdit::Character_Vector[i].Char_Name;
								if (PalEdit::Character_Vector[i].ID < 3) {
									Display_Name += " (Player 1)";
								}
								else {
									Display_Name += " (Player 2)";
								}

								if (ImGui::Selectable(Display_Name.c_str(), is_selected)) {
									// Сохраняем ID выбранного персонажа
									PalEdit::current_character_idx = PalEdit::Character_Vector[i].ID;
									PalEdit::Read_Character();
								}
								if (is_selected) {
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}						
						if (PalEdit::current_character_idx != -1) {
							Character& currentChar = PalEdit::Character_Vector[PalEdit::FindVectorIndexByID(PalEdit::current_character_idx)];
							int displayValue = currentChar.Current_Pallete_Num + 1;
							if (ImGui::SliderInt("Palette number##", &displayValue, 1, currentChar.Max_Pallete_Num)) { //Слайдер выбора палетки
								{

									currentChar.Current_Pallete_Num = displayValue - 1;
									PalEdit::ChangePallete();
									PalEdit::Read_Character();
								};
							};
							ImGui::Separator();

							if (ImGui::CollapsingHeader("Advanced")) {
								ImGui::TextWrapped("Apply effect changes before saving the palette.");
								PalEdit::DrawAdvanced();
							}
							ImGui::Separator();

							// Предварительные вычисления цветов (остаются без изменений)
							__int32& LineColor = currentChar.LineColor;
							float LinecolorFloat[4] = {
								((LineColor >> 16) & 0xFF) / 255.0f,
								((LineColor >> 8) & 0xFF) / 255.0f,
								(LineColor & 0xFF) / 255.0f,
								((LineColor >> 24) & 0xFF) / 255.0f
							};

							__int32& i32SuperShadow1 = currentChar.SuperShadowColor1;
							float fSuperShadow1[4] = {
								((i32SuperShadow1 >> 16) & 0xFF) / 255.0f,
								((i32SuperShadow1 >> 8) & 0xFF) / 255.0f,
								(i32SuperShadow1 & 0xFF) / 255.0f,
								((i32SuperShadow1 >> 24) & 0xFF) / 255.0f
							};

							__int32& i32SuperShadow2 = currentChar.SuperShadowColor2;
							float fSuperShadow2[4] = {
								((i32SuperShadow2 >> 16) & 0xFF) / 255.0f,
								((i32SuperShadow2 >> 8) & 0xFF) / 255.0f,
								(i32SuperShadow2 & 0xFF) / 255.0f,
								((i32SuperShadow2 >> 24) & 0xFF) / 255.0f
							};

							// Создаем таблицу с 2 колонками
							if (ImGui::BeginTable("ColorSettings", 2, ImGuiTableFlags_SizingFixedFit))
							{
								ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthFixed, 200.0f);
								ImGui::TableSetupColumn("Colors", ImGuiTableColumnFlags_WidthFixed, 150.0f);

								// Первая строка: Don't display character
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								if (ImGui::Checkbox("Don't display character", &PalEdit::bNODisplayChar)) {
									PalEdit::NODisplayChar();
								}

								ImGui::TableSetColumnIndex(1);
								if (ImGui::ColorEdit3("Line Color", LinecolorFloat, ImGuiColorEditFlags_NoInputs)) {
									__int32 LineColorValue =
										(static_cast<__int32>(LinecolorFloat[3] * 255) << 24) |
										(static_cast<__int32>(LinecolorFloat[0] * 255) << 16) |
										(static_cast<__int32>(LinecolorFloat[1] * 255) << 8) |
										(static_cast<__int32>(LinecolorFloat[2] * 255));

									currentChar.LineColor = LineColorValue;
									PalEdit::ChangeLineColor();
									PalEdit::Read_Character();
								}

								// Вторая строка: Don't display shadows
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								if (ImGui::Checkbox("Don't display shadows", &PalEdit::bNODisplayShadows)) {
									PalEdit::NODisplayShadow();
								}

								ImGui::TableSetColumnIndex(1);
								if (ImGui::ColorEdit3("Super Shadow 1", fSuperShadow1, ImGuiColorEditFlags_NoInputs)) {
									__int32 ColorEdit =
										(static_cast<__int32>(fSuperShadow1[3] * 255) << 24) |
										(static_cast<__int32>(fSuperShadow1[0] * 255) << 16) |
										(static_cast<__int32>(fSuperShadow1[1] * 255) << 8) |
										(static_cast<__int32>(fSuperShadow1[2] * 255));

									currentChar.SuperShadowColor1 = ColorEdit;
									PalEdit::ChangeSuperShadow1();
									PalEdit::Read_Character();
								}

								// Третья строка: Display super shadow
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								if (ImGui::Checkbox("Display super shadow", &PalEdit::bDisplaySuperShadows)) {
									PalEdit::DisplaySuperShadow();
								}

								ImGui::TableSetColumnIndex(1);
								if (ImGui::ColorEdit3("Super Shadow 2", fSuperShadow2, ImGuiColorEditFlags_NoInputs)) {
									__int32 ColorEdit =
										(static_cast<__int32>(fSuperShadow2[3] * 255) << 24) |
										(static_cast<__int32>(fSuperShadow2[0] * 255) << 16) |
										(static_cast<__int32>(fSuperShadow2[1] * 255) << 8) |
										(static_cast<__int32>(fSuperShadow2[2] * 255));

									currentChar.SuperShadowColor2 = ColorEdit;
									PalEdit::ChangeSuperShadow2();
									PalEdit::Read_Character();
								}
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0);
								auto it = GroupColorGroup::characterGroups.find(currentChar.Char_Name); //Если есть JSON с Частями персонжей
								if (it != GroupColorGroup::characterGroups.end() && !it->second.empty()) {
									bJSONEnable = true;
									ImGui::Checkbox("Group Character Parts", &bGrouping);
								}
								ImGui::EndTable();
							}


							ImGui::Text("Color Palettes: %d", currentChar.Num_Of_Color);
							ImGui::Separator();
							if (bJSONEnable and bGrouping) {
								auto it = GroupColorGroup::characterGroups.find(currentChar.Char_Name);
								// Отображаем с группировкой
								for (const auto& group : it->second) {
									// Добавляем ImGuiTreeNodeFlags_DefaultOpen для открытого состояния по умолчанию
									if (ImGui::CollapsingHeader(group.groupName.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
										// Используем стиль без отступов для более плотного расположения
										ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

										// Отображаем цвета в группе
										for (int i = group.startIndex;
											i < group.startIndex + group.count && i < currentChar.Character_Colors.size();
											i++) {

											__int32& colorValue = currentChar.Character_Colors[i];

											float colorFloat[4] = {
												((colorValue >> 16) & 0xFF) / 255.0f,
												((colorValue >> 8) & 0xFF) / 255.0f,
												(colorValue & 0xFF) / 255.0f,
												((colorValue >> 24) & 0xFF) / 255.0f
											};

											ImGui::PushID(i);
											if (ImGui::ColorEdit4(("##Color_" + std::to_string(i)).c_str(),
												colorFloat,
												ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar)) {
												colorValue =
													(static_cast<__int32>(colorFloat[3] * 255) << 24) |
													(static_cast<__int32>(colorFloat[0] * 255) << 16) |
													(static_cast<__int32>(colorFloat[1] * 255) << 8) |
													(static_cast<__int32>(colorFloat[2] * 255));

												PalEdit::ChangeColor(i, colorValue);
												PalEdit::Read_Character();
											}

											if (ImGui::IsItemHovered()) {
												ImGui::SetTooltip("Index: %d", i);
											}

											ImGui::PopID();

											// Используем SameLine() с проверкой, помещается ли следующий элемент
											bool isLastInGroup = (i == group.startIndex + group.count - 1);
											bool isLastValidColor = (i == currentChar.Character_Colors.size() - 1);

											if (!isLastInGroup && !isLastValidColor) {
												ImGui::SameLine();
											}
										}
										ImGui::Separator();
										std::string wheelKey = currentChar.Char_Name + std::string("|") + group.groupName;
										std::string btn_id = std::string("wheelBtn_") + std::to_string(group.startIndex);
										ImGui::PushID(btn_id.c_str());
										if (ImGui::Button(("Open Wheel##" + btn_id).c_str())) {
											wheelOpenMap[wheelKey] = !wheelOpenMap[wheelKey];
										}
										ImGui::PopID();

										// If the wheel is open for this group, delegate rendering to ColorWheel implementation
										auto itOpen = wheelOpenMap.find(wheelKey);
										if (itOpen != wheelOpenMap.end() && itOpen->second) {
											bool& openRef = itOpen->second;
											ColorWheel::Draw(currentChar, group, openRef);
										}

										ImGui::PopStyleVar();
									}

								}
							} //Если JSON нет.
							else {
								for (int i = 1; i < currentChar.Character_Colors.size(); i++) {
									// Получаем ссылку на 32-битный цвет
									__int32& colorValue = currentChar.Character_Colors[i];

									// Распаковываем компоненты (формат ARGB)
									float colorFloat[4] = {
										((colorValue >> 16) & 0xFF) / 255.0f,  // Red
										((colorValue >> 8) & 0xFF) / 255.0f,   // Green
										(colorValue & 0xFF) / 255.0f,          // Blue
										((colorValue >> 24) & 0xFF) / 255.0f   // Alpha
									};

									ImGui::PushID(i);
									// Отображаем ColorEdit
									if (ImGui::ColorEdit4(("Color##" + std::to_string(i)).c_str(), colorFloat, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaBar)) {

										// Упаковываем обратно в ARGB формат
										colorValue =
											(static_cast<__int32>(colorFloat[3] * 255) << 24) |  // Alpha
											(static_cast<__int32>(colorFloat[0] * 255) << 16) |  // Red
											(static_cast<__int32>(colorFloat[1] * 255) << 8) |   // Green
											(static_cast<__int32>(colorFloat[2] * 255));         // Blue

										PalEdit::ChangeColor(i, colorValue);
										PalEdit::Read_Character();
									}

									ImGui::PopID();

									if (i > 0 and i % 16 != 0) {
										ImGui::SameLine();
									}
								}
							}

						}




				}
			}
			ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Auto-load")) {
                        AutoPallete::Draw();
                        ImGui::EndTabItem();
                    }
				}
				ImGui::EndTabBar();
				ImGui::End();
		}
	}
}

