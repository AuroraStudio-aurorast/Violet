#pragma once
#include "CVeDuiButton.h"
#include "CVeDuiEdit.h"
#include "CVeDuiTabList.h"
#include "CVeDuiPlayList.h"
class CPageList : public CPage
{
private:
	constexpr static int DefCoverIndex{};
	struct LIST_INFO
	{
		ComPtr<eck::CD2DImageList> pIl;
	};

	struct TSKPARAM_LOAD_META_DATA
	{
		std::shared_ptr<CPlayList> pList;
		int idxBeginDisplay{};
		int idxEndDisplay{};
		ComPtr<eck::CD2DImageList> pIl;
		std::vector<int> vItem;
    };;

	CVeEdit m_EDSearch{};
	CVeTabList m_TBLPlayList{};
	eck::CLinearLayoutV m_LytPlayList{};

	CVeButton m_BTAddFile{};
	CVeButton m_BTLocate{};
	Dui::CLabel m_LATopBarSpacer{};
	eck::CLayoutDummy m_TopBarDummySpace{};
	CVeEdit m_EDSearchItem{};
	eck::CLinearLayoutH m_LytTopBar{};
	CVePlayList m_GLList{};
	eck::CLinearLayoutV m_LytList{};

	eck::CLinearLayoutH m_Lyt{};

	eck::CRefStrW m_rsDispInfoBuf{};

	int m_cxIl{}, m_cyIl{};
	ID2D1Bitmap1* m_pBmpDefCover{};
	std::vector<LIST_INFO> m_vListInfo{};

    eck::CoroTask<void> TskLoadSongData(TSKPARAM_LOAD_META_DATA&& Param);

	CPlayList* GetCurrPlayList();
	std::shared_ptr<CPlayList> GetCurrPlayListShared();

	HRESULT OnMenuAddFile(CPlayList* pList, int idxInsert = -1);

	void UpdateDefCover();

	void ReCreateImageList(int idx, BOOL bForce);

	void LoadMetaData(int idxBegin, int idxEnd, int idxList = -1);

	void CheckVisibleItemMetaData(int idxList);
public:
	LRESULT OnEvent(UINT uMsg, WPARAM wParam, LPARAM lParam) override;
};