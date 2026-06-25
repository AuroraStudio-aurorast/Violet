#pragma once
#include "CVeDuiScrollBar.h"

class CVeCardCon : public Dui::CElem
{
public:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};

class CVeCard : public Dui::CElem
{
private:
	int m_nType = 0;
	std::wstring m_lpTitle1{}, m_lpTitle2{};
	ID2D1Bitmap1* m_pBmp{};
	ID2D1SolidColorBrush* m_pBrush{};
	CVeCardCon m_pToolCon{};
	CVeCardCon m_pFoldCon{};
	float m_fTitleHeight = 55;
	bool m_bFold = false;
	float m_fToolConWidth = 0;
	float m_fFoldConHeight = 0;

public:
	void SetType(int type) //-1.滚木 0.普通 1.按钮 2.可折叠
	{
		m_nType = type;
	}
	void SetTitle(std::wstring_view title1, std::wstring_view title2)
	{
		ECK_DUILOCK;
		m_lpTitle1 = title1;
		m_lpTitle2 = title2;
	}
	void SetIcon(ID2D1Bitmap1* pBmp)
	{
		ECK_DUILOCK;
		std::swap(m_pBmp, pBmp);
		if (m_pBmp)
			m_pBmp->AddRef();
		if (pBmp)
			pBmp->Release();
	}
	void SetToolConWidth(float width)
	{
		m_fToolConWidth = width;
	}
	void SetFoldConHeight(float height)
	{
		m_fFoldConHeight = height;
	}
	Dui::CElem* GetToolCon() 
	{
		return &m_pToolCon;
	}
	Dui::CElem* GetFoldCon()
	{
		return &m_pFoldCon;
	}
	void SetFold(bool fold) 
	{
		m_bFold = fold;
	}
	float GetCardHeight() const
	{
		switch (m_nType)
		{
		case -1:
			return m_fFoldConHeight;
		case 0:
		case 1:
			return m_fTitleHeight;
		case 2:
			return m_fTitleHeight + (m_bFold ? 0 : m_fFoldConHeight);
		}
		return 0;
	}

	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};

class CVeCardGroup : public Dui::CElem
{
private:
	CVeScrollBar m_SB{};
	eck::CInertialScrollView* m_psv{};
	std::vector<CVeCard*> m_pCards;
	float m_fTopOffset, m_fBottomOffset;
	float m_fListHeight;
	float m_fListPos;

public:
	void SetOffset(float TopOffset, float BottomOffset)
	{
		m_fTopOffset = TopOffset;
		m_fBottomOffset = BottomOffset;
	}
	size_t AddCard(int type)
	{
		ECK_DUILOCK;
		auto card = new CVeCard;
		card->Create(nullptr, Dui::DES_VISIBLE, 0, 0, 0, 0, 0, this);
		card->SetType(type);
		m_pCards.push_back(card);
		m_SB.SetZOrder(ECK_ELEMTOP);
		return m_pCards.size() - 1;
	}
	void DelCard(size_t index)
	{
		ECK_DUILOCK;
		m_pCards[index]->Destroy();
		delete m_pCards[index];
		m_pCards.erase(m_pCards.begin() + index);
	}
	void SetCardTitle(size_t index, std::wstring_view title1, std::wstring_view title2)
	{
		ECK_DUILOCK;
		m_pCards[index]->SetTitle(title1, title2);
	}
	void SetCardIcon(size_t index, ID2D1Bitmap1* pBmp) 
	{
		ECK_DUILOCK;
		m_pCards[index]->SetIcon(pBmp);
	}
	void SetCardToolConWidth(size_t index, float width)
	{
		ECK_DUILOCK;
		m_pCards[index]->SetToolConWidth(width);
	}
	void SetCardFoldConHeight(size_t index, float height)
	{
		ECK_DUILOCK;
		m_pCards[index]->SetFoldConHeight(height);
	}
	Dui::CElem* GetCardToolCon(size_t index)
	{
		return m_pCards[index]->GetToolCon();
	}
	Dui::CElem* GetCardFoldCon(size_t index)
	{
		return m_pCards[index]->GetFoldCon();
	}

	void CardUpdatePos();
	void MouseWheel(WPARAM iWheelDelta);
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};