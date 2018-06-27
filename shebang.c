/*
 * Proj: shebang
 * File: shebang.c
 * Auth: MatveyT
 * Date: 17.06.2018
 * Desc: Enables executing MSYS shell scripts from Windows(R) command line
 * Note: Rename or symlink to match the desired script and put both on PATH
 */


/** Build instructions:

    -DUNICODE = builds 'unicode' version instead of 'ansi'
    -DNOSTDLIB (also requires -O) = no libc dependencies, useful for GCC -nostdlib build

**/


#if __STDC_VERSION__ < 199901L && !defined(__POCC__)
#error C99 compiler required.
#endif


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>


#ifndef __GNUC__
#pragma comment(lib, "shlwapi.lib")
#endif // __GNUC__


// GCC/MinGW has no libstrsafe.a separate from libmingwex.a,
// so we have to inline it in order to get rid of libc
#ifdef NOSTDLIB
#ifdef __GNUC__
#ifdef __NO_INLINE__
#error Option -O is required.
#endif // __NO_INLINE__
#define _FORCE_INLINE inline __attribute__ ((always_inline))
#ifdef UNICODE
_FORCE_INLINE HRESULT WINAPI StringCchLengthW(LPCWSTR,size_t,size_t*);
_FORCE_INLINE HRESULT WINAPI StringLengthWorkerW(LPCWSTR,size_t,size_t*);
_FORCE_INLINE HRESULT WINAPI StringCchCopyW(LPWSTR,size_t,LPCWSTR);
_FORCE_INLINE HRESULT WINAPI StringCopyWorkerW(LPWSTR,size_t,LPCWSTR);
_FORCE_INLINE HRESULT WINAPI StringCchCopyNW(LPWSTR,size_t,LPCWSTR,size_t);
_FORCE_INLINE HRESULT WINAPI StringCopyNWorkerW(LPWSTR,size_t,LPCWSTR,size_t);
_FORCE_INLINE HRESULT WINAPI StringCchCatW(LPWSTR,size_t,LPCWSTR);
_FORCE_INLINE HRESULT WINAPI StringCatWorkerW(LPWSTR,size_t,LPCWSTR);
#else
_FORCE_INLINE HRESULT WINAPI StringCchLengthA(LPCSTR,size_t,size_t*);
_FORCE_INLINE HRESULT WINAPI StringLengthWorkerA(LPCSTR,size_t,size_t*);
_FORCE_INLINE HRESULT WINAPI StringCchCopyA(LPSTR,size_t,LPCSTR);
_FORCE_INLINE HRESULT WINAPI StringCopyWorkerA(LPSTR,size_t,LPCSTR);
_FORCE_INLINE HRESULT WINAPI StringCchCopyNA(LPSTR,size_t,LPCSTR,size_t);
_FORCE_INLINE HRESULT WINAPI StringCopyNWorkerA(LPSTR,size_t,LPCSTR,size_t);
_FORCE_INLINE HRESULT WINAPI StringCchCatA(LPSTR,size_t,LPCSTR);
_FORCE_INLINE HRESULT WINAPI StringCatWorkerA(LPSTR,size_t,LPCSTR);
#endif // UNICODE
#endif // __GNUC__
#endif // NOSTDLIB
#include <strsafe.h>


// our original name
#define PROGRAM_NAME    "shebang"
// macro to facilitate function calling
#define COUNT(a)        (sizeof(a) / sizeof(*a))
#define ARRAY(a)        (a), COUNT(a)
// latin letter test
#define IS_LATIN(c)     (('A' <= (c) && (c) <= 'Z') || ('a' <= (c) && (c) <= 'z'))


// known MSYS types
typedef enum {
    MSYS_UNKNOWN = -1,
    MSYS_MSYS,      // MSYS
    MSYS_MINGW32,   // MINGW32
    MSYS_MINGW64,   // MINGW64
    MSYS_COUNT
} MSYS;


// concats TCHAR string with UTF-8 string
BOOL concat_with_utf8(PTSTR pszDest, size_t cchDest, const char* src)
{
    size_t cnt;
    if (FAILED(StringCchLength(pszDest, cchDest, &cnt)))
        return FALSE;

#ifdef UNICODE
    return (MultiByteToWideChar(CP_UTF8, 0, src, -1, pszDest + cnt, cchDest - cnt) > 0);
#else
    wchar_t tmp[cchDest - cnt]; // VLA
    return (MultiByteToWideChar(CP_UTF8, 0, src, -1, tmp, cchDest - cnt) > 0
        && WideCharToMultiByte(CP_ACP, 0, tmp, -1, pszDest + cnt, cchDest - cnt,
            NULL, NULL) > 0);
#endif // UNICODE
}


// replaces all occurences of a char in a string
void replace_char(PTSTR psz, TCHAR cTo, TCHAR cFrom)
{
    for ( ; *psz; ++psz)
        if (*psz == cFrom)
            *psz = cTo;
}


// finds active MSYS installation by scanning PATH
MSYS find_msys(PTSTR pszRoot, size_t cchRoot)
{
    // get PATH
    TCHAR achPATH[4096]; // max
    size_t cchPATH = (size_t)GetEnvironmentVariable(TEXT("PATH"), ARRAY(achPATH));
    if (!cchPATH || cchPATH > COUNT(achPATH))
        return MSYS_UNKNOWN; // unexpected error

    ++cchPATH; // count null-terminating character
    replace_char(achPATH, TEXT('\0'), TEXT(';')); // split PATH

    // find first path matching one of the patterns
    MSYS msys = MSYS_UNKNOWN;
    TCHAR* cp;
    size_t cch;
    for (cp = achPATH; cchPATH; cp += cch + 1, cchPATH -= cch + 1) {
        if (FAILED(StringCchLength(cp, cchPATH, &cch)))
            break; // unexpected error
        if (PathMatchSpec(cp, TEXT("*\\msys*\\usr\\bin"))) {
            // found MSYS
            msys = MSYS_MSYS;
            cch -= COUNT("\\usr\\bin") - 1;
            break;
        }
        if (PathMatchSpec(cp, TEXT("*\\msys*\\mingw32\\bin"))) {
            // found MINGW32
            msys = MSYS_MINGW32;
            cch -= COUNT("\\mingw32\\bin") - 1;
            break;
        }
        if (PathMatchSpec(cp, TEXT("*\\msys*\\mingw64\\bin"))) {
            // found MINGW64
            msys = MSYS_MINGW64;
            cch -= COUNT("\\mingw64\\bin") - 1;
            break;
        }
    }

    if (msys != MSYS_UNKNOWN) // wipe last two dirs to get the root
        StringCchCopyN(pszRoot, cchRoot, cp, cch);
    return msys;
}


// sets some of MSYS environment variables
void setup_msys_env(MSYS msys)
{
    // MSYS names and POSIX prefixes
    static PCTSTR const pszMSYS[MSYS_COUNT][2] = {
        { TEXT("MSYS"), TEXT("/usr") },
        { TEXT("MINGW32"), TEXT("/mingw32") },
        { TEXT("MINGW64"), TEXT("/mingw64") }
    };

    // set MSYSTEM, MSYSTEM_PREFIX and MINGW_PREFIX
    SetEnvironmentVariable(TEXT("MSYSTEM"), pszMSYS[msys][0]);
    SetEnvironmentVariable(TEXT("MSYSTEM_PREFIX"), pszMSYS[msys][1]);
    if (msys == MSYS_MINGW32 || msys == MSYS_MINGW64)
        SetEnvironmentVariable(TEXT("MINGW_PREFIX"), pszMSYS[msys][1]);

    // set USER and HOSTNAME
    TCHAR tmp[256]; // max
    if (GetEnvironmentVariable(TEXT("USERNAME"), ARRAY(tmp)))
        SetEnvironmentVariable(TEXT("USER"), tmp);
    if (GetComputerNameEx(ComputerNameDnsHostname, tmp, &(DWORD) { COUNT(tmp) }))
        SetEnvironmentVariable(TEXT("HOSTNAME"), tmp);
}


// converts MSYS shell path to native path
BOOL convert_path(PTSTR pszTo, size_t cchTo, PCTSTR pszRoot, const char* from)
{
    TCHAR tmp[MAX_PATH];

    if (IS_LATIN(from[0]) && from[1] == ':') { // Win path
        tmp[0] = TEXT('\0');
    } else if (*from == '/') { // POSIX path
        ++from;

        // substitute: /c --> c:
        if (IS_LATIN(from[0]) && from[1] == '/') {
            tmp[0] = (TCHAR)from[0];
            tmp[1] = TEXT(':');
            tmp[2] = TEXT('/');
            tmp[3] = TEXT('\0');
            from += COUNT("c/") - 1;
        } else {
            // start from MSYS root
            StringCchCopy(ARRAY(tmp), pszRoot);
            StringCchCat(ARRAY(tmp), TEXT("/"));

            // substitute: /bin --> /usr/bin
            if (from[0] == 'b' && from[1] == 'i' && from[2] == 'n' && from[3] == '/') {
                StringCchCat(ARRAY(tmp), TEXT("usr/bin/"));
                from += COUNT("bin/") - 1;
            }
        }
    } else { // relative path
        GetCurrentDirectory(COUNT(tmp), tmp);
        StringCchCat(ARRAY(tmp), TEXT("/"));
    }

    // add the rest
    if (!concat_with_utf8(ARRAY(tmp), from))
        return FALSE;

    // apply native separators and .exe extension
    replace_char(tmp, TEXT('\\'), TEXT('/')); // to Win separators
    PathAddExtension(tmp, NULL); // note: PathCchAddExtension is for Win 8+ only

    // check if file exists
    if (!PathFileExists(tmp))
        return FALSE;

    // executable name containing spaces should be enquoted for security reasons
    PathQuoteSpaces(tmp);

    // copy out result
    return SUCCEEDED(StringCchCopy(pszTo, cchTo, tmp));
}


// parses a shebang line
BOOL parse_line(char* line, size_t cnt, const char** ppc1, const char** ppc2)
{
    char* cp;

    // #!
    if (cnt < 2 || *line++ != '#' || *line++ != '!')
        return FALSE;
    cnt -= 2;

    // convert tabs to spaces, break at a newline
    for (cp = line; cnt; ++cp, --cnt) {
        if (*cp == '\0') // unexpected end of string
            return FALSE;
        if (*cp == '\n' || *cp == '\r') {
            *cp = '\0';
            break;
        }
        if (*cp == '\t')
            *cp = ' ';
    }
    if (!cnt) // no newline
        return FALSE;

    // skip leading spaces
    for (cp = line; *cp == ' '; ++cp) ;
    if (*cp == '\0') // shell not found
        return FALSE;

    // store pointer to shell name
    *ppc1 = cp;

    // skip until next space
    while (*cp && *cp != ' ') ++cp;

    if (*cp == ' ') {
        // have args
        *cp++ = '\0';
        *ppc2 = cp;
    } else {
        // no args
        *ppc2 = NULL;
    }

    return TRUE;
}


// checks if file is a shell script
BOOL can_shebang(PCTSTR pszScriptName, PTSTR pszShellName, size_t cchShellName,
    PCTSTR pszRoot, PDWORD pdwErrorCode)
{
    // open script file
    HANDLE hScriptFile = CreateFile(pszScriptName, GENERIC_READ, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hScriptFile == INVALID_HANDLE_VALUE) {
        *pdwErrorCode = GetLastError();
        return FALSE;
    }

    BOOL shebang = FALSE;
    char buf[cchShellName]; // VLA
    DWORD cb;
    const char *pc1, *pc2;
    if (!ReadFile(hScriptFile, buf, cchShellName, &cb, NULL)) {
        // cannot read file
        *pdwErrorCode = GetLastError();
    } else if (!parse_line(buf, (size_t)cb, &pc1, &pc2)) {
        // not a script
        *pdwErrorCode = ERROR_BAD_FORMAT;
    } else if (!convert_path(pszShellName, cchShellName, pszRoot, pc1)) {
        // invalid shell
        *pdwErrorCode = ERROR_PATH_NOT_FOUND;
    } else {
        // process shebang args if any
        shebang = pc2 ? (
            SUCCEEDED(StringCchCat(pszShellName, cchShellName, TEXT(" "))) &&
            concat_with_utf8(pszShellName, cchShellName, pc2)
        ) : TRUE;
        if (!shebang) // invalid shebang args
            *pdwErrorCode = ERROR_BAD_ARGUMENTS;
    }

    CloseHandle(hScriptFile);
    return shebang;
}


// prints error message and quits the application
__declspec(noreturn)
void print_error_and_exit(DWORD dwErrorCode)
{
    LPTSTR pszErrorText;
    DWORD cchErrorText = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
        dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&pszErrorText,
        0, NULL);
    if (cchErrorText > 0) {
        HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
        DWORD cb;
        WriteConsole(hStdErr, ARRAY(TEXT(PROGRAM_NAME " error: ")) - 1, &cb, NULL);
        WriteConsole(hStdErr, pszErrorText, cchErrorText, &cb, NULL);
        // no need to free memory, as we're going to exit anyway
        //HeapFree(GetProcessHeap(), 0, pszErrorText);
    }
    ExitProcess(dwErrorCode);
}


// application standard entry point
#ifdef UNICODE
int wmain(void)
#else
int main(void)
#endif // UNICODE
{
    DWORD dwErrorCode;

#ifndef UNICODE
    // get rid of OEM codepage
    SetConsoleOutputCP(GetACP());
#endif // UNICODE

    // find our basename
    TCHAR szName[MAX_PATH];
    GetModuleFileName(NULL, ARRAY(szName));
    PathStripPath(szName);
    PathRemoveExtension(szName); // note: PathCchRemoveExtension is for Win 8+ only

    // check if we're renamed
    if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, szName, -1,
        TEXT(PROGRAM_NAME), -1) == CSTR_EQUAL) {
        WriteConsole(GetStdHandle(STD_ERROR_HANDLE), ARRAY(TEXT(
"Hello Windows(R) world! It\'s me, humble \'" PROGRAM_NAME "\' utility.\n\n"
"I can help you to execute MSYS shell scripts from your native \'cmd\',\n"
"but only if you already have MSYS on your PATH.\n\n"
"All you have to do is to rename or symlink me, so that I match the script you want.\n"
"And, of course, please, make sure that we\'re both on the PATH too.\n\n"
"Let\'s do it!\n\n"
            )) - 1, &(DWORD) { 0 }, NULL);
        print_error_and_exit(ERROR_CANT_RESOLVE_FILENAME);
    }

    // find MSYS root
    TCHAR szMSYSRoot[MAX_PATH];
    MSYS msys = find_msys(ARRAY(szMSYSRoot));
    if (msys == MSYS_UNKNOWN)
        print_error_and_exit(ERROR_INVALID_ENVIRONMENT);

    // find matching shell script on PATH
    if (!PathFindOnPath(szName, NULL))
        print_error_and_exit(ERROR_FILE_NOT_FOUND);

    // can she bang?
    TCHAR szShellCmd[MAX_PATH];
    if (!can_shebang(szName, ARRAY(szShellCmd), szMSYSRoot, &dwErrorCode))
        print_error_and_exit(dwErrorCode);

    // prepare script name for passing onto the shell
    PathQuoteSpaces(szName);
    replace_char(szName, TEXT('/'), TEXT('\\')); // to POSIX separators

    // make the command line
    TCHAR szCmdLine[32768]; // max
    StringCchCopy(ARRAY(szCmdLine), szShellCmd);    // shell + shebang args
    StringCchCat(ARRAY(szCmdLine), TEXT(" "));      // space
    StringCchCat(ARRAY(szCmdLine), szName);         // script
    PTSTR pszRawArgs = PathGetArgs(GetCommandLine());
    if (pszRawArgs && *pszRawArgs) {
        StringCchCat(ARRAY(szCmdLine), TEXT(" "));  // space
        StringCchCat(ARRAY(szCmdLine), pszRawArgs); // our args
    }

    // setup MSYS environment
    setup_msys_env(msys);

    // launch the shell
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcess(NULL, szCmdLine, NULL, NULL, FALSE, 0, NULL, NULL,
        &(STARTUPINFO) { .cb = sizeof(STARTUPINFO) }, &pi))
        print_error_and_exit(GetLastError());

    // wait for the child and exit
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &dwErrorCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)dwErrorCode;
}
