#include "main.hpp"
#include "jniutils.hpp"
#include <cstdint>
#include <jni.h>
#include <string>
#include <thread>
#include <map>

static ModInfo modInfo;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo, LoggerOptions(false, true));
    return *logger;
}

#include "UnityEngine/Color.hpp"
#include "TMPro/TMP_SpriteAsset.hpp"

using namespace UnityEngine;
using namespace TMPro;

bool added = false;

constexpr int EMOJI_SIZE = 108;
constexpr int SHEET_TILES = 10;
constexpr int SHEET_SIZE = SHEET_TILES * EMOJI_SIZE;

int currentEmojiIndex;
bool textureNeedsApply;
ArrayW<Color> clearPixels;
TMP_SpriteAsset* rootEmojiAsset;
TMP_SpriteAsset* currentEmojiAsset;

struct ThreadGlobals {
    jobject bitmap;
    jobject canvas;
    jobject paint;
};
std::map<std::thread::id, ThreadGlobals> jobjects = {};

#include <sstream>

std::string utf8ToInt(int unicode) {
    std::stringstream strm;
    strm << unicode;
    return strm.str();
}

std::string utf8ToHex(int unicode) {
    std::stringstream stream;
    stream << std::hex << unicode;
    return stream.str();
}

#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Resources.hpp"

TMP_SpriteAsset* CreateSpriteAsset() {
    auto texture = Texture2D::New_ctor(SHEET_SIZE, SHEET_SIZE, TextureFormat::RGBA32, false);
    texture->SetPixels(clearPixels);
    texture->Apply(false, false);

    auto spriteAsset = ScriptableObject::CreateInstance<TMP_SpriteAsset*>();
    spriteAsset->fallbackSpriteAssets = List<TMP_SpriteAsset*>::New_ctor();
    spriteAsset->spriteInfoList = List<TMP_Sprite*>::New_ctor();
    spriteAsset->spriteSheet = texture;

    auto mat = Resources::FindObjectsOfTypeAll<Material*>().FirstOrDefault([](auto x) { return x->get_name() == "Teko-Medium SDF Curved Softer"; });
    spriteAsset->material = Object::Instantiate(mat);
    spriteAsset->material->set_mainTexture(spriteAsset->spriteSheet);

    return spriteAsset;
}

#include "TMPro/TMP_Sprite.hpp"
#include "TMPro/TMP_SpriteGlyph.hpp"
#include "TMPro/TMP_SpriteCharacter.hpp"

TMP_SpriteGlyph* PushSprite(int unicode) {
    if (currentEmojiIndex >= SHEET_TILES * SHEET_TILES) {
        auto newSheet = CreateSpriteAsset();
        rootEmojiAsset->fallbackSpriteAssets->Add(newSheet);
        currentEmojiAsset = newSheet;
        currentEmojiIndex = 0;
    }

    std::string text = utf8ToInt(unicode);

    int column = currentEmojiIndex % SHEET_TILES;
    int row = currentEmojiIndex / SHEET_TILES;

    auto glyph = TMP_SpriteGlyph::New_ctor();
    glyph->set_index(currentEmojiIndex);
    glyph->set_metrics(TextCore::GlyphMetrics(EMOJI_SIZE, EMOJI_SIZE, 0.25 * EMOJI_SIZE, 0.75 * EMOJI_SIZE, EMOJI_SIZE));
    glyph->set_glyphRect(TextCore::GlyphRect(column * EMOJI_SIZE, SHEET_SIZE - ((row + 1) * EMOJI_SIZE), EMOJI_SIZE, EMOJI_SIZE));

    if (!currentEmojiAsset->m_SpriteGlyphTable)
        currentEmojiAsset->m_SpriteGlyphTable = List<TMP_SpriteGlyph*>::New_ctor();
    currentEmojiAsset->m_SpriteGlyphTable->Add(glyph);

    TMP_SpriteCharacter* character = TMP_SpriteCharacter::New_ctor(unicode, glyph);
    character->set_name(text);
    character->set_scale(1);

    if (!currentEmojiAsset->m_SpriteCharacterTable)
        currentEmojiAsset->m_SpriteCharacterTable = List<TMP_SpriteCharacter*>::New_ctor();
    currentEmojiAsset->m_SpriteCharacterTable->Add(character);

    currentEmojiAsset->SortGlyphTable();
    currentEmojiAsset->UpdateLookupTables();

    return glyph;
}

#include <android/bitmap.h>
#include <GLES3/gl32.h>
// #include "UnityEngine/ImageConversion.hpp"

void DrawTexture(uint unicode, TMP_SpriteGlyph* glyph) {
    auto env = JNIUtils::GetJNIEnv();

    auto id = std::this_thread::get_id();
    if (!jobjects.contains(id))
        jobjects[id] = {};
    auto globals = jobjects[id];

    if (!globals.bitmap) {
        GET_JCLASS(env, configClass, "android/graphics/Bitmap$Config");
        GET_STATIC_JOBJECT_FIELD(env, config, configClass, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
        GET_JCLASS(env, bitmapClass, "android/graphics/Bitmap");
        CALL_STATIC_JOBJECT_METHOD(env, tmpBitmap, bitmapClass, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;", EMOJI_SIZE, EMOJI_SIZE, config);
        globals.bitmap = env->NewGlobalRef(tmpBitmap);
    }
    if (!globals.canvas) {
        GET_JCLASS(env, canvasClass, "android/graphics/Canvas");
        NEW_JOBJECT(env, tmpCanvas, canvasClass, "(Landroid/graphics/Bitmap;)V", globals.bitmap);
        globals.canvas = env->NewGlobalRef(tmpCanvas);
    }
    if (!globals.paint) {
        GET_JCLASS(env, paintClass, "android/graphics/Paint");
        NEW_JOBJECT(env, tmpPaint, paintClass, "()V");
        CALL_VOID_METHOD(env, tmpPaint, "setTextSize", "(F)V", (jfloat) EMOJI_SIZE * 0.75);
        CALL_VOID_METHOD(env, tmpPaint, "setAntiAlias", "(Z)V", true);
        CALL_VOID_METHOD(env, tmpPaint, "setARGB", "(IIII)V", 255, 255, 255, 255);
        globals.paint = env->NewGlobalRef(tmpPaint);
    }

    CALL_VOID_METHOD(env, globals.bitmap, "eraseColor", "(I)V", 0);
    jstring str;
    int l = 1;
    if (unicode < 0xffff)
        str = env->NewString((jchar*) &unicode, 1);
    else if (unicode < 0x10ffff) {
        uint n = unicode - 0x10000;
        jchar tmp[2];
        tmp[0] = 0xd800 | (n >> 10);
        tmp[1] = 0xdc00 | (n & 0x3ff);
        str = env->NewString((jchar*) tmp, 2);
        l = 2;
    } else {
        getLogger().info("too big unicode %i", unicode);
        return;
    }
    CALL_VOID_METHOD(env, globals.canvas, "drawText", "(Ljava/lang/String;IIFFLandroid/graphics/Paint;)V", str, 0, l, (jfloat) 0, (jfloat) EMOJI_SIZE * 0.8, globals.paint);

    // debug: saves bitmap with drawn text
    // static bool saved = false;

    // if (!saved) {
    //     GET_JCLASS(env, foutClass, "java/io/FileOutputStream");
    //     NEW_JOBJECT(env, fout, foutClass, "(Ljava/lang/String;)V", env->NewStringUTF("/sdcard/bitmap.png"));
    //     GET_JCLASS(env, cmpClass, "android/graphics/Bitmap$CompressFormat");
    //     GET_STATIC_JOBJECT_FIELD(env, cmp, cmpClass, "PNG", "Landroid/graphics/Bitmap$CompressFormat;");
    //     CALL_JBOOLEAN_METHOD(env, savebool, globals.bitmap, "compress", "(Landroid/graphics/Bitmap$CompressFormat;ILjava/io/OutputStream;)Z", cmp, 100, fout);
    //     saved = true;
    // }

    uint32_t* pixels = nullptr;
    AndroidBitmap_lockPixels(env, globals.bitmap, (void**) &pixels);

    // ARGB8888: 4 sets of 8 bits per pixel
    int pixelsSizeU32 = EMOJI_SIZE * EMOJI_SIZE;

    // find boundaries and flip vertically
    uint32_t flippedPixels[pixelsSizeU32];
    int left = EMOJI_SIZE;
    int right = 0;
    int lower = EMOJI_SIZE;
    int upper = 0;

    for (int row = 0; row < EMOJI_SIZE; row++) {
        int rowOff = row * EMOJI_SIZE;
        int flipRowOff = EMOJI_SIZE * (EMOJI_SIZE - row - 1);
        bool foundPix = false;
        for (int col = EMOJI_SIZE - 1; col >= 0; col--) {
            auto pix = pixels[flipRowOff + col];
            if (pix != 0) {
                foundPix = true;
                if (col > right)
                    right = col;
                if (col < left)
                    left = col;
            }
            flippedPixels[rowOff + col] = pix;
        }
        if (foundPix && row < lower)
            lower = row;
        if (foundPix && row > upper)
            upper = row;
    }

    AndroidBitmap_unlockPixels(env, globals.bitmap);

    auto tex = Texture2D::New_ctor(EMOJI_SIZE, EMOJI_SIZE, TextureFormat::RGBA32, false);
    tex->LoadRawTextureData(flippedPixels, pixelsSizeU32 * 4); // size in uint8_t*
    tex->Apply();

    // debug: saves texure after copy
    // static bool saved2 = false;

    // if (!saved2) {
    //     auto bytes = ImageConversion::EncodeToPNG(tex);
    //     writefile("/sdcard/char.png", std::string((char*) bytes.begin(), bytes.Length()));
    //     saved2 = true;
    //     getLogger().info("l %i r %i b %i t %i", left, right, lower, upper);
    // }

    int column = currentEmojiIndex % SHEET_TILES;
    int row = currentEmojiIndex / SHEET_TILES;
    int x = column * EMOJI_SIZE;
    int y = SHEET_SIZE - ((row + 1) * EMOJI_SIZE);

    static auto CopyTexture_Region = il2cpp_utils::resolve_icall<void, UnityEngine::Texture*, int, int, int, int, int, int, UnityEngine::Texture*, int, int, int, int>
        ("UnityEngine.Graphics::CopyTexture_Region");
    CopyTexture_Region(tex, 0, 0, 0, 0, tex->get_width(), tex->get_height(), currentEmojiAsset->spriteSheet, 0, 0, x, y);

    // add a little bit of spacing
    right = std::min(right + (EMOJI_SIZE / 10), EMOJI_SIZE);
    int width = right - left;
    if (width < 0)
        width = 0;
    x += left;
    int height = EMOJI_SIZE;
    // int height = upper - lower;
    // if (height < 0)
    //     height = 0;
    // y += lower;
    glyph->set_metrics(TextCore::GlyphMetrics(width, height, 0.125 * EMOJI_SIZE, 0.875 * EMOJI_SIZE, width));
    glyph->set_glyphRect(TextCore::GlyphRect(x, y, width, height));

    currentEmojiAsset->SortGlyphTable();
    currentEmojiAsset->UpdateLookupTables();
}

#include "GlobalNamespace/MainFlowCoordinator.hpp"
#include "UnityEngine/Font.hpp"
#include "TMPro/TMP_Settings.hpp"
#include "TMPro/TMP_FontAsset.hpp"

using namespace GlobalNamespace;

MAKE_HOOK_MATCH(MainFlowCoordinator_DidActivate, &MainFlowCoordinator::DidActivate, void, MainFlowCoordinator* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {

    if (!added) {
        static auto Font_GetDefault = il2cpp_utils::resolve_icall<Font*>
            ("UnityEngine.Font::GetDefault");
        auto asset = TMP_FontAsset::CreateFontAsset(Font_GetDefault());
        TMP_Settings::get_fallbackFontAssets()->Add(asset);

        clearPixels = ArrayW<Color>(SHEET_SIZE * SHEET_SIZE);
        // for (size_t i = 0; i < SHEET_SIZE * SHEET_SIZE; i++)
        //     clearPixels[i] = Color(0, 0, 0, 0);

        rootEmojiAsset = CreateSpriteAsset();
        currentEmojiAsset = rootEmojiAsset;
        currentEmojiIndex = 0;
        TMP_Settings::get_instance()->m_defaultSpriteAsset = rootEmojiAsset;

        added = true;
    }

    MainFlowCoordinator_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
}

// #include "UnityEngine/RenderTexture.hpp"
// #include "UnityEngine/Graphics.hpp"
// #include "UnityEngine/ImageConversion.hpp"

MAKE_HOOK_MATCH(TMP_SpriteAsset_SearchForSpriteByUnicode, &TMP_SpriteAsset::SearchForSpriteByUnicode, TMP_SpriteAsset*, TMP_SpriteAsset* spriteAsset, uint unicode, bool includeFallbacks, ByRef<int> spriteIndex) {

    auto result = TMP_SpriteAsset_SearchForSpriteByUnicode(spriteAsset, unicode, includeFallbacks, spriteIndex);

    if (!result) {
        getLogger().debug("unicode %i", unicode);

        auto glyph = PushSprite(unicode);

        DrawTexture(unicode, glyph);

        result = currentEmojiAsset;
        *spriteIndex = currentEmojiIndex++;

        // debug: saves whole sprite sheet
        // auto tex = (Texture2D*) currentEmojiAsset->spriteSheet;
        // auto tmp = RenderTexture::GetTemporary(tex->get_width(), tex->get_height(), 0, RenderTextureFormat::ARGB32, RenderTextureReadWrite::Linear);
        // Graphics::Blit(tex, tmp);
        // auto previous = RenderTexture::get_active();
        // RenderTexture::set_active(tmp);
        // auto readable = Texture2D::New_ctor(tex->get_width(), tex->get_height());
        // readable->ReadPixels({0, 0, (float) tex->get_width(), (float) tex->get_height()}, 0, 0, false);
        // readable->Apply();
        // RenderTexture::set_active(previous);
        // RenderTexture::ReleaseTemporary(tmp);
        // auto bytes = ImageConversion::EncodeToPNG(readable);
        // writefile("/sdcard/sprites.png", std::string((char*) bytes.begin(), bytes.Length()));
    }

    return result;
}

extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;

    getLogger().info("Attaching JNI");
    JNIEnv* env = Modloader::getJni();
    env->GetJavaVM(&JNIUtils::Jvm);

    getLogger().info("Completed setup!");
}

extern "C" void load() {
    il2cpp_functions::Init();

    getLogger().info("Installing hooks...");
    INSTALL_HOOK(getLogger(), MainFlowCoordinator_DidActivate);
    INSTALL_HOOK(getLogger(), TMP_SpriteAsset_SearchForSpriteByUnicode);
    getLogger().info("Installed all hooks!");
}
