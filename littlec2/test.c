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
	int test;
	test = 12;
	puts("check\n");
	puts("pepepopochechk\n");
	int l;
	l = 12;
	testFunc(l);

    print("Test is:"); //test
    print(test);

    while(test <= 15)
    {
        print("\nTesting while\n");
        print(test);
        test = test + 1;
    }

	return 0; /*test*/
}