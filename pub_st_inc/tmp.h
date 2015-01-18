	double i=1.25;
	unsigned long long a;
	unsigned long long m,e,v,v1,t=1;
	int cnt=0,ll;
	memcpy(&a,&i,8);
	m=a&mask_mm|0x0010000000000000;
	if(m!=0)
	{
		while((m&0xffff)==0){m=m>>16;cnt+=16;};
		while((m&0xf)==0){m=m>>4;cnt+=4;};
		while((m&1)==0){m=m>>1;cnt++;};
	}
	e=a>>52;
	ll=(52-cnt-(e-1023));
	v=ll>0?m>>ll:m<<(0-ll);
	v1=ll>0?((~((~0)<<ll))&m):0;
