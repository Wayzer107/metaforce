#include <utility>

#define NOD_ATHENA 1
#include "SpecBase.hpp"
#include "DNAMP1/PAK.hpp"
#include "DNAMP1/MLVL.hpp"
#include "DNAMP1/STRG.hpp"

namespace Retro
{

struct SpecMP1 : SpecBase
{
    std::vector<std::pair<std::string, DNAMP1::PAK>> m_paks;

    bool checkFromGCNDisc(NOD::DiscGCN& disc,
                          const std::vector<const HECL::SystemString*>& args,
                          std::vector<ExtractReport>& reps)
    {
        if (memcmp(disc.getHeader().gameID, "GM8", 3))
            return false;
        char region = disc.getHeader().gameID[3];
        static const std::string regNONE = "";
        static const std::string regE = "NTSC";
        static const std::string regJ = "NTSC-J";
        static const std::string regP = "PAL";
        const std::string* regstr = &regNONE;
        switch (region)
        {
        case 'E':
            regstr = &regE;
            break;
        case 'J':
            regstr = &regJ;
            break;
        case 'P':
            regstr = &regP;
            break;
        }

        NOD::DiscGCN::IPartition* partition = disc.getDataPartition();
        std::unique_ptr<uint8_t[]> dolBuf = partition->getDOLBuf();
        const char* buildInfo = (char*)memmem(dolBuf.get(), partition->getDOLSize(), "MetroidBuildInfo", 16) + 16;

        reps.emplace_back();
        ExtractReport& rep = reps.back();
        rep.name = "MP1";
        rep.desc = "Metroid Prime " + *regstr;
        if (buildInfo)
            rep.desc += " (" + std::string(buildInfo) + ")";

        /* Iterate PAKs and build level options */
        std::map<std::string, std::pair<const NOD::DiscBase::IPartition::Node*, DNAMP1::PAK*>,
                CaseInsensitiveCompare> orderedPaks;
        NOD::DiscBase::IPartition::Node& root = disc.getDataPartition()->getFSTRoot();
        for (const NOD::DiscBase::IPartition::Node& child : root)
        {
            const std::string& name = child.getName();
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), tolower);
            if (name.size() > 4)
            {
                std::string::iterator extit = lowerName.end() - 4;
                if (!std::string(extit, lowerName.end()).compare(".pak"))
                {
                    /* This is a pak */
                    std::string lowerBase(lowerName.begin(), extit);

                    /* Needs filter */
                    bool good = true;
                    if (args.size())
                    {
                        good = false;
                        if (!lowerName.compare(0, 7, "metroid"))
                        {
                            HECL::SystemChar idxChar = lowerName[7];
                            for (const HECL::SystemString* arg : args)
                            {
                                if (arg->size() == 1 && iswdigit((*arg)[0]))
                                    if ((*arg)[0] == idxChar)
                                        good = true;
                            }
                        }

                        if (!good)
                        {
                            for (const HECL::SystemString* arg : args)
                            {
#if HECL_UCS2
                                std::string lowerArg = HECL::WideToUTF8(*arg);
#else
                                std::string lowerArg = *arg;
#endif
                                std::transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), tolower);
                                if (!lowerArg.compare(0, lowerBase.size(), lowerBase))
                                    good = true;
                            }
                        }
                    }

                    if (good)
                    {
                        m_paks.emplace_back(std::make_pair(name, DNAMP1::PAK()));
                        std::pair<std::string, DNAMP1::PAK>& res = m_paks.back();
                        NOD::AthenaPartReadStream rs(child.beginReadStream());
                        res.second.read(rs);
                        orderedPaks[name] = std::make_pair(&child, &res.second);
                    }
                }
            }
        }

        for (std::pair<std::string, std::pair<const NOD::DiscBase::IPartition::Node*, DNAMP1::PAK*>> item : orderedPaks)
        {
            rep.childOpts.emplace_back();
            ExtractReport& childRep = rep.childOpts.back();
            childRep.name = item.first;

            for (DNAMP1::PAK::Entry& entry : *item.second.second)
            {
                static const HECL::FourCC MLVLfourcc("MLVL");
                if (entry.type == MLVLfourcc)
                {
                    NOD::AthenaPartReadStream rs(item.second.first->beginReadStream(entry.offset));
                    DNAMP1::MLVL mlvl;
                    mlvl.read(rs);
                    const DNAMP1::PAK::Entry* nameEnt = item.second.second->lookupEntry(mlvl.worldNameId);
                    if (nameEnt)
                    {
                        DNAMP1::STRG mlvlName;
                        rs.seek(nameEnt->offset, Athena::Begin);
                        mlvlName.read(rs);
                        if (childRep.desc.size())
                            childRep.desc += _S(", ");
#if HECL_UCS2
                        childRep.desc += mlvlName.langs[0].strings[0];
#else
                        childRep.desc += HECL::WideToUTF8(mlvlName.langs[0].strings[0]);
#endif
                    }
                }
            }
        }

        return true;
    }
    bool readFromGCNDisc(NOD::DiscGCN& disc,
                         const std::vector<const HECL::SystemString*>& args)
    {

    }

    bool checkFromWiiDisc(NOD::DiscWii& disc,
                          const std::vector<const HECL::SystemString*>& args,
                          std::vector<ExtractReport>& reps)
    {
        if (memcmp(disc.getHeader().gameID, "R3M", 3))
            return false;
        return true;
    }
    bool readFromWiiDisc(NOD::DiscWii& disc,
                         const std::vector<const HECL::SystemString*>& args)
    {
    }

    bool checkFromProject(HECL::Database::Project& proj)
    {
    }
    bool readFromProject(HECL::Database::Project& proj)
    {
    }

    bool visitGameObjects(std::function<bool(const HECL::Database::ObjectBase&)>)
    {
    }
    struct LevelSpec : public ILevelSpec
    {
        bool visitLevelObjects(std::function<bool(const HECL::Database::ObjectBase&)>)
        {
        }
        struct AreaSpec : public IAreaSpec
        {
            bool visitAreaObjects(std::function<bool(const HECL::Database::ObjectBase&)>)
            {
            }
        };
        bool visitAreas(std::function<bool(const IAreaSpec&)>)
        {
        }
    };
    bool visitLevels(std::function<bool(const ILevelSpec&)>)
    {
    }
};

HECL::Database::DataSpecEntry SpecMP1 =
{
    _S("MP1"),
    _S("Data specification for original Metroid Prime engine"),
    [](HECL::Database::DataSpecTool) -> HECL::Database::IDataSpec* {return new struct SpecMP1;}
};

}


