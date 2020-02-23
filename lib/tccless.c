//Weird file for stubbing out tinycc builds where you don't want compile-time stuff.

void CNOVRStopTCCSystem()
{
}

void CNOVRTCCLog( void * data, const char * tolog )
{
	puts( tolog );
}

int tcc_add_symbol(void *s1, const char *name, const void *val)
{

}
