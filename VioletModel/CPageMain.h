#pragma once
#include "CVeDuiButton.h"
class CPageMain : public CPage
{
private:
	CVeButton m_BTOpenFile{};
	CVeButton m_BTOpenFolder{};
	eck::CLayoutDummy m_Dummy{};
	Dui::CLabel m_LATest{};

	eck::CLinearLayoutH m_Lyt{};
public:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};