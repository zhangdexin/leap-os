#include "apilib.h"

// ?文件以shift-jts??保存或者EUC??保存
void HariMain(void)
{
	static char s[9] = { 0xb2, 0xdb, 0xca, 0xc6, 0xce, 0xcd, 0xc4, 0x0a, 0x00 };
		/* 半角 */
	//api_putstr0(s);
	//api_putstr0("私は最高です  あ");
	api_putstr0("日本語EUCで書いてみたよー");
	// api_putstr0("你好呀");
	api_end();
}