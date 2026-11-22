#ifndef __PRESENT_DIAG_H__
#define __PRESENT_DIAG_H__

// 一時計測プローブ (環境変数 TVP_PRESENT_DIAG にダンプ先パス)。計測後は撤去する。
// 実装は VSyncTimingThread.cpp に一元化する。inline 関数の static ローカルは
// MSVC /GL で TU 毎に複製され、リングとタイムスタンプが破綻するため。
namespace PresentDiag
{
	double NowMs();
	void Emit(const char* tag, double t_ms, const char* fmt, ...);
}

#endif // __PRESENT_DIAG_H__
