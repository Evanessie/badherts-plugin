/* Minimal VST3 smoke test: load the module and instantiate the plugin factory.
   Deliberately avoids the VST3 SDK so it can be built with a bare gcc. */
#include <windows.h>
#include <stdio.h>

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        printf ("usage: hosttest <module.vst3>\n");
        return 64;
    }

    HMODULE module = LoadLibraryA (argv[1]);

    if (module == NULL)
    {
        printf ("FAIL LoadLibrary error=%lu\n", (unsigned long) GetLastError());
        return 1;
    }

    void* (*getFactory) (void) = (void* (*) (void)) (void (*) (void)) GetProcAddress (module, "GetPluginFactory");

    if (getFactory == NULL)
    {
        printf ("FAIL GetPluginFactory not exported\n");
        return 2;
    }

    void* factory = getFactory();

    if (factory == NULL)
    {
        printf ("FAIL GetPluginFactory returned null\n");
        return 3;
    }

    printf ("OK factory=%p\n", factory);
    FreeLibrary (module);
    return 0;
}
