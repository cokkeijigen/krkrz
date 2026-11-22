//---------------------------------------------------------------------------
// GPU 合成 共通部
//---------------------------------------------------------------------------
#include "tjsCommHead.h"
#include "GPUComposite.h"
#include "SysInitIntf.h"
#include "drawable.h"

#include <map>
#include <stdlib.h>

//---------------------------------------------------------------------------
// 既定で有効。バックバッファ比較で CPU 合成と画素単位で一致することを確認済み
// (最大差 2/255、平均 0.03)。-gcomp=0 / TVP_GPU_COMPOSITE=0 で従来動作
int TVPGPUCompositeMode = 1;
static bool g_option_parsed = false;

// 亜像素平滑の master switch。既定は有効だが、実際に連続化するのは
// TJS が Layer.enableSubpixelMotion() を呼んだ層だけ (UI は対象外)。
// TVP_GPU_SMOOTH=0 で全体を無効化できる
bool TVPGPUSmoothEnabled()
{
	static int on = -1;
	if(on < 0)
	{
		char s[8] = {0};
		on = 1;
		if(::GetEnvironmentVariableA("TVP_GPU_SMOOTH", s, 7) > 0 && s[0] == '0') on = 0;
	}
	return on != 0;
}

void TVPParseGPUCompositeOption()
{
	if(g_option_parsed) return;
	g_option_parsed = true;

	// 環境変数 (テスト用)
	const char * env = ::getenv("TVP_GPU_COMPOSITE");
	if(env != NULL && env[0] != '\0')
	{
		TVPGPUCompositeMode = (env[0] == '0') ? 0 : 1;
		return;
	}

	// コマンドライン -gcomp=0
	tTJSVariant opt;
	if(TVPGetCommandLine(TJS_W("-gcomp"), &opt))
	{
		ttstr s(opt);
		TVPGPUCompositeMode = (s.length() > 0 && s[0] == TJS_W('0')) ? 0 : 1;
	}
}
//---------------------------------------------------------------------------
tjs_uint TVPGPUCompositeFrameNo = 1;
tjs_uint TVPGPUCompositeFrame() { return TVPGPUCompositeFrameNo; }
//---------------------------------------------------------------------------
bool TVPGPUBlendSupported(tjs_int type)
{
	switch((tTVPLayerType)type)
	{
	case ltOpaque:
	case ltAlpha:
	case ltAdditive:
	case ltSubtractive:
	case ltMultiplicative:
		return true;
	default:
		return false;
	}
}
//---------------------------------------------------------------------------
// マネージャ毎の quad リスト
//---------------------------------------------------------------------------
static std::map<void *, std::vector<TVPGPUQuad> > g_quad_lists;
static std::map<void *, std::vector<TVPGPUGroup> > g_groups;
static std::map<void *, std::vector<int> > g_group_stack;

std::vector<TVPGPUQuad> & TVPGPUCompositeGetQuads(iTVPLayerManager * manager)
{
	return g_quad_lists[manager];
}
//---------------------------------------------------------------------------
bool TVPGPUCompositeHasQuads(iTVPLayerManager * manager)
{
	std::map<void *, std::vector<TVPGPUQuad> >::iterator it =
		g_quad_lists.find(manager);
	return it != g_quad_lists.end() && !it->second.empty();
}
//---------------------------------------------------------------------------
void TVPGPUCompositeClearQuads(iTVPLayerManager * manager)
{
	std::map<void *, std::vector<TVPGPUQuad> >::iterator it =
		g_quad_lists.find(manager);
	if(it != g_quad_lists.end()) it->second.clear();
}
//---------------------------------------------------------------------------
void TVPGPUCompositeClearGroups(iTVPLayerManager * manager)
{
	g_groups[manager].clear();
	g_group_stack[manager].clear();
}
//---------------------------------------------------------------------------
int TVPGPUCompositeBeginGroup(iTVPLayerManager * manager, tjs_int opacity, tjs_int blendType)
{
	std::vector<TVPGPUGroup> & groups = g_groups[manager];
	std::vector<int> & stack = g_group_stack[manager];
	TVPGPUGroup g{};
	g.Opacity = opacity;
	g.BlendType = blendType;
	g.QuadStart = static_cast<int>(g_quad_lists[manager].size());
	g.QuadEnd = g.QuadStart;
	groups.push_back(g);
	const int id = static_cast<int>(groups.size()) - 1;
	stack.push_back(id);
	return id;
}
//---------------------------------------------------------------------------
void TVPGPUCompositeEndGroup(iTVPLayerManager * manager)
{
	std::vector<int> & stack = g_group_stack[manager];
	if(stack.empty()) return;
	const int id = stack.back();
	g_groups[manager][id].QuadEnd = static_cast<int>(g_quad_lists[manager].size());
	stack.pop_back();
}
//---------------------------------------------------------------------------
int TVPGPUCompositeCurrentGroup(iTVPLayerManager * manager)
{
	std::vector<int> & stack = g_group_stack[manager];
	return stack.empty() ? -1 : stack.back();
}
//---------------------------------------------------------------------------
const std::vector<TVPGPUGroup> & TVPGPUCompositeGetGroups(iTVPLayerManager * manager)
{
	return g_groups[manager];
}
//---------------------------------------------------------------------------
// レイヤ破棄通知
//---------------------------------------------------------------------------
static tTVPGPULayerPurgeFn g_layer_purge = NULL;

void TVPGPUCompositeSetLayerPurge(tTVPGPULayerPurgeFn fn)
{
	g_layer_purge = fn;
}
//---------------------------------------------------------------------------
void TVPGPUCompositeOnLayerDestroyed(void * layer)
{
	if(g_layer_purge) g_layer_purge(layer);
}
