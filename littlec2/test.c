int testFunc(int num)
{
	int sum, i;
	sum = 0;
	print(0);
	for (i = 1; i < num; i = i + 1)
	{
		print("+");
		print(i);
		sum = sum + i;
	}
	print("=");
	print(sum);
	puts("");

	return 0;
}
int main()
{
	int te;
	te = 12;
	puts("check");
	puts("pepepopochechk");
	int l;
	l = 12;
	testFunc(l);
	return 0;
}