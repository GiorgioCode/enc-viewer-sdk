// portrayal.cpp — Catálogo de reglas y diccionarios descriptivos de clases y atributos S-57.
// Los diccionarios se leen una sola vez desde los recursos de GDAL, sin rutas del checkout.
#include <cpl_conv.h>
#include <cpl_csv.h>
#include <enc/portrayal.h>
#include <fstream>
#include <map>
#include <sstream>
namespace enc {
std::vector<PortrayalRule> defaultPortrayalRules() {
    using C = DisplayCategory;
    std::vector<PortrayalRule> out;
    for (const auto* name : {"LNDARE", "DEPARE", "DRGARE", "DEPCNT", "COALNE"})
        out.push_back({name, C::Base, 1, {}, {}, 1, LinePattern::Solid});
    for (const auto* name : {"BOYLAT", "BOYCAR", "BOYISD", "BOYSAW", "BOYSPP", "BCNLAT", "BCNCAR",
                             "BCNSPP", "LIGHTS", "TOPMAR", "WRECKS", "OBSTRN", "UWTROC", "SLCONS",
                             "LNDELV", "RESARE", "CTNARE", "TSSBND", "TSSLPT", "FAIRWY"})
        out.push_back({name, C::Standard, 6, {}, {}, 1, LinePattern::Solid});
    for (const auto* name : {"SOUNDG", "BUAARE", "BUISGL", "CBLARE", "CBLSUB", "PIPARE", "PIPSOL",
                             "ACHARE", "LAKARE", "RIVERS", "DOCARE"})
        out.push_back({name, C::Other, 4, {}, {}, 1, LinePattern::Solid});
    for (auto& r : out)
        if (r.objectClass == "RESARE" || r.objectClass == "CTNARE" || r.objectClass == "CBLSUB" ||
            r.objectClass == "PIPSOL")
            r.pattern = LinePattern::Dashed;
    return out;
}
namespace {
std::vector<std::string> csv(const std::string& line) {
    std::vector<std::string> fields;
    std::string value;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const auto c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                value += '"';
                ++i;
            } else
                quoted = !quoted;
        } else if (c == ',' && !quoted) {
            fields.push_back(value);
            value.clear();
        } else if (c != '\r')
            value += c;
    }
    fields.push_back(value);
    return fields;
}
struct Dictionaries {
    std::map<std::string, std::string> classes, attributes, codes;
    Dictionaries() {
        for (const auto* file : {"s57objectclasses.csv", "s57attributes.csv"}) {
            const char* path = CSVFilename(file);
            if (!path)
                continue;
            std::ifstream input(path);
            std::string line;
            std::getline(input, line);
            while (std::getline(input, line)) {
                auto f = csv(line);
                if (f.size() < 3)
                    continue;
                (std::string(file) == "s57attributes.csv" ? attributes : classes)[f[2]] = f[1];
                if (std::string(file) == "s57attributes.csv")
                    codes[f[0]] = f[2];
            }
        }
        if (const char* path = CSVFilename("s57expectedinput.csv")) {
            std::ifstream input(path);
            std::string line;
            std::getline(input, line);
            while (std::getline(input, line)) {
                const auto f = csv(line);
                if (f.size() >= 3 && codes.count(f[0]))
                    meanings[codes[f[0]] + ":" + f[1]] = f[2];
            }
        }
    }
    std::map<std::string, std::string> meanings;
};
const Dictionaries& dict() {
    static const Dictionaries d;
    return d;
}
std::string lookup(const std::map<std::string, std::string>& map, const std::string& key) {
    const auto it = map.find(key);
    return it == map.end() ? key : it->second;
}
} // namespace
std::string objectClassDescription(const std::string& name) {
    return lookup(dict().classes, name);
}
std::string attributeDescription(const std::string& name) {
    return lookup(dict().attributes, name);
}
std::vector<AttributeDescription> describeAttributes(const std::vector<Attribute>& attributes) {
    std::vector<AttributeDescription> out;
    for (const auto& a : attributes) {
        AttributeDescription d{a.name, attributeDescription(a.name), a.value, {}};
        std::istringstream input(a.value);
        std::string token;
        while (std::getline(input, token, ',')) {
            if (!d.meaning.empty())
                d.meaning += ", ";
            d.meaning += lookup(dict().meanings, a.name + ":" + token);
        }
        if (d.meaning.find(a.name + ":") != std::string::npos)
            d.meaning.clear();
        out.push_back(std::move(d));
    }
    return out;
}
} // namespace enc
