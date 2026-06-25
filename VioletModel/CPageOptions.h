#pragma once
#include "CVeDuiCard.h"
#include "CVeDuiImage.h"
#include "CApp.h"

class CPageOptions : public CPage
{
private:
	eck::CLinearLayoutH m_Lyt{};

	CVeTabList m_TBLOpt{};
	eck::CLinearLayoutV m_LytSidebar{};

	int m_nPage;
	CVeCardGroup* m_Pages[4];
	eck::CLinearLayoutH m_LytPage{};
	LPCWSTR m_lpTitle[4];
	ID2D1Bitmap1* m_pImg[4];

	CVeCardGroup m_GenOpt{};
	CVeCardGroup m_SkinOpt{};
	CVeCardGroup m_AboutOpt{};
	CVeImage m_AboutOptLogo{};
	CVeImage m_AboutOptStudioLogo{};
	CVeButton m_BTAboutOptRgUUID{};

public:
	void SwitchPage(int index);
	void UpdateTitle();
	void UpdateImg();
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};