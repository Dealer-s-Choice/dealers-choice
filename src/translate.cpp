#include <tinygettext/tinygettext.hpp>
#include <tinygettext/unix_file_system.hpp>
#include <string>
#include <memory>
#include <stdio.h>

static std::unique_ptr<tinygettext::DictionaryManager> dm;
static tinygettext::Dictionary* dict = nullptr;

extern "C" void init_translation(const char *lang, const char *path) {
    // Re-initialize manager with file system
    dm = std::make_unique<tinygettext::DictionaryManager>(
        std::unique_ptr<tinygettext::FileSystem>(new tinygettext::UnixFileSystem)
    );

    dm->add_directory(path);

    tinygettext::Language language = tinygettext::Language::from_spec(lang);
    dm->set_language(language);

    dict = &dm->get_dictionary();

    if (!dict) {
        printf("Failed to load dictionary for language: %s\n", lang);
        return;
    }

    const char *test_msgid = "Connect";
    std::string test_result = dict->translate(test_msgid);
    printf("Test translate '%s' => '%s'\n", test_msgid, test_result.c_str());
}

extern "C" const char *translate(const char *msgid) {
    static std::string result;
    if (dict) {
        result = dict->translate(msgid);
        printf("Translated '%s' to '%s'\n", msgid, result.c_str());
        return result.c_str();
    }
    return msgid;
}
