/*
 *   tadssettings_portable.cpp - non-Windows (file-backed) backend for the
 *   guit3 settings store (tadssettings.h)
 *
 *   This is the cross-platform counterpart of tadssettings_w32.cpp, which
 *   implements the same CTadsSettings interface on top of the Windows
 *   registry.  A flat key/value file has no native subkey enumeration the
 *   way the registry does, so this backend keeps the whole store as an
 *   in-memory tree of key nodes (built once from the file, on the first
 *   call) and simply rewrites the entire file after every change - the
 *   store is a handful of small preference values, never large or written
 *   under time pressure, so this is much simpler than incremental on-disk
 *   updates and just as reliable.  CMake selects exactly one backend per
 *   build (htmltads/imgui/CMakeLists.txt).  See migration.md 5.4/C.
 *
 *   File format: a small INI-like text file, one section per key path
 *   (backslash-delimited, exactly as CTadsSettings::open_key() receives it -
 *   there is no reason to translate the separator for a file only this code
 *   reads):
 *
 *       [Software\TADS\HTML TADS\3.0\Settings]
 *       SomeStringValue=hello
 *       [Software\TADS\HTML TADS\3.0\Settings\Profiles\Plain Text]
 *       SomeBinaryValue:bin=48656c6c6f
 *
 *   A ":bin" suffix on the name marks a hex-encoded REG_BINARY-equivalent
 *   value (set_key_binary()/query_key_binary(); used for one value, the
 *   custom-color swatch array, htmlpref.cpp's custclr_val_name); every other
 *   value is a plain string (REG_SZ-equivalent - covers query/set_key_long()
 *   and query/set_key_bool() too, exactly as the registry backend stores
 *   them as decimal/"Yes"/"No" strings).  Value names in this codebase are
 *   fixed C string literals with no '=' or newline in them, so no name
 *   escaping is needed; string *values* can contain arbitrary text, so '\'
 *   and newlines are backslash-escaped on write and reversed on read.
 *
 *   The store lives at $XDG_CONFIG_HOME/HTML TADS 3/settings.ini (falling
 *   back to $HOME/.config on Linux) or ~/Library/Preferences/HTML TADS 3/
 *   settings.ini on macOS - "HTML TADS 3" matches w32_appdata_dir
 *   (guitrt3.cpp), the same app-identity string already used for the
 *   Windows saved-game folder.
 */

#ifdef _WIN32
#error "tadssettings_portable.cpp is the non-Windows backend; Windows builds use tadssettings_w32.cpp"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#ifndef TADSHTML_H
#include "tadshtml.h"
#endif
#ifndef TADSSETTINGS_H
#include "tadssettings.h"
#endif

/*
 *   w32_appdata_dir (guitrt3.cpp) - the app identity string already used for
 *   the Windows saved-game folder ("HTML TADS 3"), reused here as the
 *   settings directory name.  Declared directly rather than via guimain.h,
 *   which also declares a page of Win32-only prototypes that need
 *   tadsplat.h; this file has no reason to pull those in.
 */
extern const char w32_appdata_dir[];


/* ------------------------------------------------------------------------ */
/*
 *   A single value under a key: either a string or a hex-encoded binary
 *   blob, matching the two REG_* types the Win32 backend actually uses.
 */
struct SettingsValue
{
    std::string name;
    bool is_binary;
    std::string str_val;                  /* valid when !is_binary */
    std::vector<unsigned char> bin_val;   /* valid when is_binary */
};

/*
 *   A key node.  Keyed by its full backslash-delimited path in the owning
 *   store's node map; children are found by scanning for that prefix (see
 *   find_children()) rather than tracked explicitly, since the profile list
 *   this is used for is at most a few dozen entries.
 */
struct SettingsNode
{
    std::vector<SettingsValue> values;
};

/* an open key handle - just remembers which node it refers to */
struct tads_settings_key_opaque
{
    std::string path;
};


/* ------------------------------------------------------------------------ */
/*
 *   The store: every key node that has ever been created (explicitly via
 *   open_key(path, TRUE), or implicitly as an ancestor of one that was),
 *   keyed by its full path.  Loaded from disk on first use and rewritten
 *   after every change.
 */
namespace {

class SettingsStore
{
public:
    static SettingsStore &get()
    {
        static SettingsStore instance;
        return instance;
    }

    /* find a node, or null if it doesn't exist */
    SettingsNode *find(const std::string &path)
    {
        std::map<std::string, SettingsNode>::iterator it = nodes_.find(path);
        return it == nodes_.end() ? 0 : &it->second;
    }

    /* find or create a node, materializing any missing ancestors */
    SettingsNode *create(const std::string &path)
    {
        /* materialize every ancestor, so subkey enumeration finds them */
        size_t pos = 0;
        while ((pos = path.find('\\', pos)) != std::string::npos)
        {
            nodes_[path.substr(0, pos)];
            ++pos;
        }

        return &nodes_[path];
    }

    /* remove a (leaf) node */
    void erase(const std::string &path) { nodes_.erase(path); }

    /*
     *   Collect the immediate child names of 'path' (the path segment past
     *   the last backslash of each child whose path starts with
     *   "path\"), in a stable sorted order.
     */
    std::vector<std::string> find_children(const std::string &path)
    {
        std::vector<std::string> result;
        std::string prefix = path + "\\";

        for (std::map<std::string, SettingsNode>::const_iterator it =
                 nodes_.begin() ; it != nodes_.end() ; ++it)
        {
            const std::string &p = it->first;
            if (p.size() <= prefix.size()
                || p.compare(0, prefix.size(), prefix) != 0)
                continue;

            /* only an immediate child - no further backslash after prefix */
            std::string rest = p.substr(prefix.size());
            if (rest.find('\\') == std::string::npos)
                result.push_back(rest);
        }

        std::sort(result.begin(), result.end());
        return result;
    }

    void load();
    void save();

private:
    SettingsStore() { load(); }

    std::map<std::string, SettingsNode> nodes_;
};

/* get the full path to the settings file, creating its directory */
std::string settings_file_path()
{
    std::string dir;

#if defined(__APPLE__)
    const char *home = getenv("HOME");
    dir = (home != 0 ? std::string(home) : std::string("."))
        + "/Library/Preferences/" + w32_appdata_dir;
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg != 0 && xdg[0] != '\0')
    {
        dir = std::string(xdg) + "/" + w32_appdata_dir;
    }
    else
    {
        const char *home = getenv("HOME");
        dir = (home != 0 ? std::string(home) : std::string("."))
            + "/.config/" + w32_appdata_dir;
    }
#endif

    /* create the directory (and its parent) if they don't exist yet */
    size_t pos = 0;
    while ((pos = dir.find('/', pos + 1)) != std::string::npos)
        mkdir(dir.substr(0, pos).c_str(), 0755);
    mkdir(dir.c_str(), 0755);

    return dir + "/settings.ini";
}

/* backslash-escape a string value for on-disk storage */
std::string escape_value(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0 ; i < s.size() ; ++i)
    {
        char c = s[i];
        if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else
            out += c;
    }
    return out;
}

std::string unescape_value(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0 ; i < s.size() ; ++i)
    {
        if (s[i] == '\\' && i + 1 < s.size())
        {
            char c = s[++i];
            out += (c == 'n' ? '\n' : c == 'r' ? '\r' : c);
        }
        else
            out += s[i];
    }
    return out;
}

std::string bin_to_hex(const unsigned char *buf, size_t len)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0 ; i < len ; ++i)
    {
        out += digits[(buf[i] >> 4) & 0xf];
        out += digits[buf[i] & 0xf];
    }
    return out;
}

std::vector<unsigned char> hex_to_bin(const std::string &hex)
{
    std::vector<unsigned char> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0 ; i + 1 < hex.size() ; i += 2)
    {
        int hi = isdigit((unsigned char)hex[i])
            ? hex[i] - '0' : (tolower(hex[i]) - 'a' + 10);
        int lo = isdigit((unsigned char)hex[i + 1])
            ? hex[i + 1] - '0' : (tolower(hex[i + 1]) - 'a' + 10);
        out.push_back((unsigned char)((hi << 4) | lo));
    }
    return out;
}

void SettingsStore::load()
{
    FILE *fp = fopen(settings_file_path().c_str(), "r");
    if (fp == 0)
        return;

    char line[4096];
    std::string cur_path;
    while (fgets(line, sizeof(line), fp) != 0)
    {
        /* strip the trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        if (len == 0)
            continue;

        if (line[0] == '[' && line[len - 1] == ']')
        {
            cur_path.assign(line + 1, len - 2);
            create(cur_path);
            continue;
        }

        if (cur_path.empty())
            continue;

        char *eq = strchr(line, '=');
        if (eq == 0)
            continue;

        std::string name(line, eq - line);
        std::string val(eq + 1);

        SettingsValue v;
        static const char bin_suffix[] = ":bin";
        size_t suflen = sizeof(bin_suffix) - 1;
        if (name.size() > suflen
            && name.compare(name.size() - suflen, suflen, bin_suffix) == 0)
        {
            v.name = name.substr(0, name.size() - suflen);
            v.is_binary = true;
            v.bin_val = hex_to_bin(val);
        }
        else
        {
            v.name = name;
            v.is_binary = false;
            v.str_val = unescape_value(val);
        }

        create(cur_path)->values.push_back(v);
    }

    fclose(fp);
}

void SettingsStore::save()
{
    FILE *fp = fopen(settings_file_path().c_str(), "w");
    if (fp == 0)
        return;

    for (std::map<std::string, SettingsNode>::const_iterator it =
             nodes_.begin() ; it != nodes_.end() ; ++it)
    {
        fprintf(fp, "[%s]\n", it->first.c_str());
        for (size_t i = 0 ; i < it->second.values.size() ; ++i)
        {
            const SettingsValue &v = it->second.values[i];
            if (v.is_binary)
                fprintf(fp, "%s:bin=%s\n", v.name.c_str(),
                        bin_to_hex(&v.bin_val[0], v.bin_val.size()).c_str());
            else
                fprintf(fp, "%s=%s\n", v.name.c_str(),
                        escape_value(v.str_val).c_str());
        }
    }

    fclose(fp);
}

/* find a value by name within a node, or null if there isn't one */
SettingsValue *find_value(SettingsNode *node, const char *name)
{
    for (size_t i = 0 ; i < node->values.size() ; ++i)
        if (node->values[i].name == name)
            return &node->values[i];
    return 0;
}

SettingsValue *set_value(SettingsNode *node, const char *name)
{
    if (SettingsValue *v = find_value(node, name))
        return v;
    node->values.push_back(SettingsValue());
    node->values.back().name = name;
    return &node->values.back();
}

} // namespace


/* ------------------------------------------------------------------------ */
tads_settings_key_t CTadsSettings::open_key(const textchar_t *path, int create)
{
    SettingsStore &store = SettingsStore::get();
    SettingsNode *node = create ? store.create(path) : store.find(path);
    if (node == 0)
        return 0;

    tads_settings_key_opaque *key = new tads_settings_key_opaque();
    key->path = path;
    return key;
}

void CTadsSettings::close_key(tads_settings_key_t key)
{
    delete (tads_settings_key_opaque *)key;
}

int CTadsSettings::value_exists(tads_settings_key_t key,
                                const textchar_t *valname)
{
    SettingsNode *node = SettingsStore::get().find(((tads_settings_key_opaque *)key)->path);
    return node != 0 && find_value(node, valname) != 0;
}

long CTadsSettings::query_key_long(tads_settings_key_t key,
                                   const textchar_t *valname)
{
    char buf[128];
    query_key_str(key, valname, buf, sizeof(buf));
    return get_atol(buf);
}

int CTadsSettings::query_key_bool(tads_settings_key_t key,
                                  const textchar_t *valname)
{
    char buf[128];
    query_key_str(key, valname, buf, sizeof(buf));
    return (buf[0] == 'y' || buf[0] == 'Y');
}

size_t CTadsSettings::query_key_str(tads_settings_key_t key,
                                    const textchar_t *valname,
                                    textchar_t *buf, size_t bufsiz)
{
    SettingsNode *node = SettingsStore::get().find(((tads_settings_key_opaque *)key)->path);
    SettingsValue *v = node == 0 ? 0 : find_value(node, valname);
    if (v == 0 || v->is_binary)
    {
        buf[0] = '\0';
        return 0;
    }

    size_t len = v->str_val.size();
    if (len >= bufsiz)
        len = bufsiz - 1;
    memcpy(buf, v->str_val.data(), len);
    buf[len] = '\0';
    return len;
}

size_t CTadsSettings::query_key_binary(tads_settings_key_t key,
                                       const textchar_t *valname,
                                       void *buf, size_t bufsiz)
{
    SettingsNode *node = SettingsStore::get().find(((tads_settings_key_opaque *)key)->path);
    SettingsValue *v = node == 0 ? 0 : find_value(node, valname);
    if (v == 0 || !v->is_binary)
        return 0;

    /* match RegQueryValueEx()'s behavior: fail rather than truncate if the
       buffer is smaller than the stored value */
    size_t len = v->bin_val.size();
    if (len > bufsiz)
        return 0;
    if (len != 0)
        memcpy(buf, &v->bin_val[0], len);
    return len;
}

void CTadsSettings::set_key_long(tads_settings_key_t key,
                                 const textchar_t *valname, long val)
{
    char buf[32];
    sprintf(buf, "%ld", val);
    set_key_str(key, valname, buf, strlen(buf));
}

void CTadsSettings::set_key_str(tads_settings_key_t key,
                                const textchar_t *valname,
                                const textchar_t *str, size_t len)
{
    SettingsStore &store = SettingsStore::get();
    SettingsNode *node = store.create(((tads_settings_key_opaque *)key)->path);
    SettingsValue *v = set_value(node, valname);
    v->is_binary = false;
    v->str_val.assign(str, len);
    store.save();
}

void CTadsSettings::set_key_bool(tads_settings_key_t key,
                                 const textchar_t *valname, int val)
{
    set_key_str(key, valname, val ? "Yes" : "No", val ? 3 : 2);
}

void CTadsSettings::set_key_binary(tads_settings_key_t key,
                                   const textchar_t *valname,
                                   void *buf, size_t bufsiz)
{
    SettingsStore &store = SettingsStore::get();
    SettingsNode *node = store.create(((tads_settings_key_opaque *)key)->path);
    SettingsValue *v = set_value(node, valname);
    v->is_binary = true;
    v->bin_val.assign((unsigned char *)buf, (unsigned char *)buf + bufsiz);
    store.save();
}

int CTadsSettings::enum_subkeys(tads_settings_key_t key, unsigned int idx,
                                textchar_t *buf, size_t bufsiz)
{
    std::vector<std::string> children =
        SettingsStore::get().find_children(((tads_settings_key_opaque *)key)->path);
    if (idx >= children.size())
        return TADS_SETTINGS_ENUM_END;

    size_t len = children[idx].size();
    if (len >= bufsiz)
        len = bufsiz - 1;
    memcpy(buf, children[idx].data(), len);
    buf[len] = '\0';
    return TADS_SETTINGS_ENUM_OK;
}

int CTadsSettings::enum_str_values(tads_settings_key_t key, unsigned int idx,
                                   textchar_t *namebuf, size_t namebufsiz,
                                   textchar_t *valbuf, size_t valbufsiz)
{
    SettingsNode *node = SettingsStore::get().find(((tads_settings_key_opaque *)key)->path);
    if (node == 0 || idx >= node->values.size())
        return TADS_SETTINGS_ENUM_END;

    const SettingsValue &v = node->values[idx];

    size_t nlen = v.name.size();
    if (nlen >= namebufsiz)
        nlen = namebufsiz - 1;
    memcpy(namebuf, v.name.data(), nlen);
    namebuf[nlen] = '\0';

    if (v.is_binary)
        return TADS_SETTINGS_ENUM_SKIP;

    size_t vlen = v.str_val.size();
    if (vlen >= valbufsiz)
        vlen = valbufsiz - 1;
    memcpy(valbuf, v.str_val.data(), vlen);
    valbuf[vlen] = '\0';
    return TADS_SETTINGS_ENUM_OK;
}

int CTadsSettings::delete_key(const textchar_t *path)
{
    SettingsStore &store = SettingsStore::get();
    store.erase(path);
    store.save();
    return 0;
}
