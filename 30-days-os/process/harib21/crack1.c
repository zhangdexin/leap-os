void api_putchar(int c);
void api_end();

void HariMain(void)
{
	// crack1 触发中断保护0x0d(一般保护)
	//*((char *) 0x00102600) = 0;
	//api_end();

	//bug1 触发中断保护0x0c(栈保护)
	// char a[100];
	// a[10] = 'A';
	// api_putchar(a[10]);
	// a[102] = 'B';
	// api_putchar(a[102]);
	// a[123] = 'C';
	// api_putchar(a[123]);

	// bug3 无限输出，测试结束应用程序
	for (;;) {
		api_putchar('a');
	}
}
