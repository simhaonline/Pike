inherit Tools.Shoot.Test;

constant name="Read binary INT16";

int n = 10000;

void perform()
{
   array_sscanf(Crypto.randomness.pike_random()->read(2*n),"%2c"*n);
}

string present_n(int ntot,int nruns,float tseconds,float useconds,int memusage)
{
   return sprintf("%.0f/s",ntot/useconds);
}
