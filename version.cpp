const char *get_qaac_version()
{
#ifdef REFALAC
    return "0.11";
#else
    return "1.03";
#endif
}
