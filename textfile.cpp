#include <vector>
#include "win32util.h"
#include <shlwapi.h>
#include <mlang.h>
#include "shared_ptr.h"

#ifdef __MINGW32__
const GUID IID_IMultiLanguage2 = { 0xdccfc164, 0x2b38, 0x11d2,
    { 0xb7, 0xec, 0x00, 0xc0, 0x4f,0x8f,0x5d,0x9a } };
#endif

inline
void throwIfError(HRESULT expr, const char *msg)
{
    if (FAILED(expr))
	throw_win32_error(msg, expr);
}
#define HR(expr) (void)(throwIfError((expr), #expr))

void release(IUnknown *x) { x->Release(); }

std::wstring load_text_file(const std::wstring &path, uint32_t codepage)
{
    IStream *stream;
    HR(SHCreateStreamOnFileW(path.c_str(),
		STGM_READ | STGM_SHARE_DENY_WRITE, &stream));
    x::shared_ptr<IStream> streamPtr(stream, release);

    LARGE_INTEGER li = { 0 };
    ULARGE_INTEGER ui;
    HR(stream->Seek(li, STREAM_SEEK_END, &ui));
    if (ui.QuadPart > 0x100000)
	throw std::runtime_error(format("%ls: file too big", path.c_str()));
    size_t fileSize = ui.QuadPart;
    HR(stream->Seek(li, STREAM_SEEK_SET, &ui));

    IMultiLanguage2 *mlang;
    HR(CoCreateInstance(CLSID_CMultiLanguage, 0, CLSCTX_INPROC_SERVER,
		IID_IMultiLanguage2, (void**)(&mlang)));
    x::shared_ptr<IMultiLanguage2> mlangPtr(mlang, release);

    DetectEncodingInfo encoding[2];
    INT nscores = 2;
    HR(mlang->DetectCodepageInIStream(0, codepage,
		stream, encoding, &nscores));
    /*
     * Usually DetectCodepageInIStream() puts the most appropriate choice
     * in the first place.
     * However, it tends to pick 1252(latin-1) for UTF-8 encoded latin-1 file.
     */
    size_t pick =
	encoding[0].nCodePage == 1252 && encoding[1].nCodePage == 65001;

    HR(stream->Seek(li, STREAM_SEEK_SET, &ui));

    std::vector<char> ibuf(fileSize);
    ULONG nread;
    HR(stream->Read(&ibuf[0], ibuf.size(), &nread));

    DWORD ctx = 0;
    UINT size = ibuf.size(), cnt;
    HR(mlang->ConvertStringToUnicode(&ctx, encoding[pick].nCodePage,
		&ibuf[0], &size, 0, &cnt));
    std::vector<wchar_t> obuf(cnt);
    size = ibuf.size();
    HR(mlang->ConvertStringToUnicode(&ctx, encoding[pick].nCodePage,
		&ibuf[0], &size, &obuf[0], &cnt));
    obuf.push_back(0);
    // chop off BOM
    size_t bom = obuf.size() && obuf[0] == 0xfeff;
    return normalize_crlf(&obuf[bom], L"\n");
}