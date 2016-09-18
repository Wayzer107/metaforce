#include "hecl/hecl.hpp"
#include "hecl/Database.hpp"
#include <regex>

namespace hecl
{
static const SystemRegex regGLOB(_S("\\*"), SystemRegex::ECMAScript|SystemRegex::optimize);
static const SystemRegex regPATHCOMP(_S("[/\\\\]*([^/\\\\]+)"), SystemRegex::ECMAScript|SystemRegex::optimize);
static const SystemRegex regDRIVELETTER(_S("^([^/]*)/"), SystemRegex::ECMAScript|SystemRegex::optimize);

static SystemString CanonRelPath(const SystemString& path)
{
    /* Tokenize Path */
    std::vector<SystemString> comps;
    hecl::SystemRegexMatch matches;
    SystemString in = path;
    SanitizePath(in);
    for (; std::regex_search(in, matches, regPATHCOMP) ; in = matches.suffix())
    {
        const SystemString& match = matches[1];
        if (!match.compare(_S(".")))
            continue;
        else if (!match.compare(_S("..")))
        {
            if (comps.empty())
            {
                /* Unable to resolve outside project */
                LogModule.report(logvisor::Fatal, _S("Unable to resolve outside project root in %s"), path.c_str());
                return _S(".");
            }
            comps.pop_back();
            continue;
        }
        comps.push_back(match);
    }

    /* Emit relative path */
    if (comps.size())
    {
        auto it = comps.begin();
        SystemString retval = *it;
        for (++it ; it != comps.end() ; ++it)
        {
            if ((*it).size())
            {
                retval += _S('/');
                retval += *it;
            }
        }
        return retval;
    }
    return _S(".");
}

static SystemString CanonRelPath(const SystemString& path, const ProjectRootPath& projectRoot)
{
    /* Absolute paths not allowed; attempt to make project-relative */
    if (IsAbsolute(path))
        return CanonRelPath(projectRoot.getProjectRelativeFromAbsolute(path));
    return CanonRelPath(path);
}

void ProjectPath::assign(Database::Project& project, const SystemString& path)
{
    m_proj = &project;

    SystemString usePath;
    size_t pipeFind = path.rfind(_S('|'));
    if (pipeFind != SystemString::npos)
    {
        m_auxInfo.assign(path.cbegin() + pipeFind + 1, path.cend());
        usePath.assign(path.cbegin(), path.cbegin() + pipeFind);
    }
    else
        usePath = path;

    m_relPath = CanonRelPath(usePath);
    m_absPath = project.getProjectRootPath().getAbsolutePath() + _S('/') + m_relPath;
    SanitizePath(m_relPath);
    SanitizePath(m_absPath);
    
    ComputeHash();
}

#if HECL_UCS2
void ProjectPath::assign(Database::Project& project, const std::string& path)
{
    std::wstring wpath = UTF8ToWide(path);
    assign(project, wpath);
}
#endif

void ProjectPath::assign(const ProjectPath& parentPath, const SystemString& path)
{
    m_proj = parentPath.m_proj;

    SystemString usePath;
    size_t pipeFind = path.rfind(_S('|'));
    if (pipeFind != SystemString::npos)
    {
        m_auxInfo.assign(path.cbegin() + pipeFind + 1, path.cend());
        usePath.assign(path.cbegin(), path.cbegin() + pipeFind);
    }
    else
        usePath = path;

    m_relPath = CanonRelPath(parentPath.m_relPath + _S('/') + usePath);
    m_absPath = m_proj->getProjectRootPath().getAbsolutePath() + _S('/') + m_relPath;
    SanitizePath(m_relPath);
    SanitizePath(m_absPath);

    ComputeHash();
}

#if HECL_UCS2
void ProjectPath::assign(const ProjectPath& parentPath, const std::string& path)
{
    std::wstring wpath = UTF8ToWide(path);
    assign(parentPath, wpath);
}
#endif

ProjectPath ProjectPath::getCookedPath(const Database::DataSpecEntry& spec) const
{
    ProjectPath woExt = getWithExtension(nullptr, true);
    ProjectPath ret(m_proj->getProjectCookedPath(spec), woExt.getRelativePath());

    if (getAuxInfo().size())
        return ret.getWithExtension((_S('.') + getAuxInfo()).c_str());
    else
        return ret;
}

ProjectPath::Type ProjectPath::getPathType() const
{
    if (std::regex_search(m_absPath, regGLOB))
    {
        std::vector<ProjectPath> globResults;
        getGlobResults(globResults);
        return globResults.size() ? Type::Glob : Type::None;
    }
    Sstat theStat;
    if (hecl::Stat(m_absPath.c_str(), &theStat))
        return Type::None;
    if (S_ISDIR(theStat.st_mode))
        return Type::Directory;
    if (S_ISREG(theStat.st_mode))
        return Type::File;
    return Type::None;
}

Time ProjectPath::getModtime() const
{
    Sstat theStat;
    time_t latestTime = 0;
    if (std::regex_search(m_absPath, regGLOB))
    {
        std::vector<ProjectPath> globResults;
        getGlobResults(globResults);
        for (ProjectPath& path : globResults)
        {
            if (!hecl::Stat(path.getAbsolutePath().c_str(), &theStat))
            {
                if (S_ISREG(theStat.st_mode) && theStat.st_mtime > latestTime)
                    latestTime = theStat.st_mtime;
            }
        }
    }
    if (!hecl::Stat(m_absPath.c_str(), &theStat))
    {
        if (S_ISREG(theStat.st_mode))
        {
            return Time(theStat.st_mtime);
        }
        else if (S_ISDIR(theStat.st_mode))
        {
            hecl::DirectoryEnumerator de(m_absPath);
            for (const hecl::DirectoryEnumerator::Entry& ent : de)
            {
                if (!hecl::Stat(ent.m_path.c_str(), &theStat))
                {
                    if (S_ISREG(theStat.st_mode) && theStat.st_mtime > latestTime)
                        latestTime = theStat.st_mtime;
                }
            }
            return Time(latestTime);
        }
    }
    LogModule.report(logvisor::Fatal, _S("invalid path type for computing modtime in '%s'"), m_absPath.c_str());
    return Time();
}

static void _recursiveGlob(Database::Project& proj,
                           std::vector<ProjectPath>& outPaths,
                           size_t level,
                           const SystemRegexMatch& pathCompMatches,
                           const SystemString& itStr,
                           bool needSlash)
{
    if (level >= pathCompMatches.size())
        return;

    SystemString comp = pathCompMatches.str(level);
    if (!std::regex_search(comp, regGLOB))
    {
        SystemString nextItStr = itStr;
        if (needSlash)
            nextItStr += _S('/');
        nextItStr += comp;
        _recursiveGlob(proj, outPaths, level+1, pathCompMatches, nextItStr, true);
        return;
    }

    /* Compile component into regex */
    SystemRegex regComp(comp, SystemRegex::ECMAScript);

    hecl::DirectoryEnumerator de(itStr);
    for (const hecl::DirectoryEnumerator::Entry& ent : de)
    {
        if (std::regex_search(ent.m_name, regComp))
        {
            SystemString nextItStr = itStr;
            if (needSlash)
                nextItStr += '/';
            nextItStr += ent.m_name;

            hecl::Sstat theStat;
            if (Stat(nextItStr.c_str(), &theStat))
                continue;

            if (ent.m_isDir)
                _recursiveGlob(proj, outPaths, level+1, pathCompMatches, nextItStr, true);
            else
                outPaths.emplace_back(proj, nextItStr);
        }
    }
}

void ProjectPath::getDirChildren(std::map<SystemString, ProjectPath>& outPaths) const
{
    hecl::DirectoryEnumerator de(m_absPath);
    for (const hecl::DirectoryEnumerator::Entry& ent : de)
        outPaths[ent.m_name] = ProjectPath(*this, ent.m_name);
}

hecl::DirectoryEnumerator ProjectPath::enumerateDir() const
{
    return hecl::DirectoryEnumerator(m_absPath);
}

void ProjectPath::getGlobResults(std::vector<ProjectPath>& outPaths) const
{
#if _WIN32
    SystemString itStr;
    SystemRegexMatch letterMatch;
    if (m_absPath.compare(0, 2, _S("//")))
        itStr = _S("\\\\");
    else if (std::regex_search(m_absPath, letterMatch, regDRIVELETTER))
        if (letterMatch[1].str().size())
            itStr = letterMatch[1];
#else
    SystemString itStr = _S("/");
#endif

    SystemRegexMatch pathCompMatches;
    if (std::regex_search(m_absPath, pathCompMatches, regPATHCOMP))
        _recursiveGlob(*m_proj, outPaths, 1, pathCompMatches, itStr, false);
}

ProjectRootPath SearchForProject(const SystemString& path)
{
    ProjectRootPath testRoot(path);
    SystemString::const_iterator begin = testRoot.getAbsolutePath().begin();
    SystemString::const_iterator end = testRoot.getAbsolutePath().end();
    while (begin != end)
    {
        SystemString testPath(begin, end);
        SystemString testIndexPath = testPath + _S("/.hecl/beacon");
        Sstat theStat;
        if (!hecl::Stat(testIndexPath.c_str(), &theStat))
        {
            if (S_ISREG(theStat.st_mode))
            {
                FILE* fp = hecl::Fopen(testIndexPath.c_str(), _S("rb"));
                if (!fp)
                    continue;
                char magic[4];
                size_t readSize = fread(magic, 1, 4, fp);
                fclose(fp);
                if (readSize != 4)
                    continue;
                static const hecl::FourCC hecl("HECL");
                if (hecl::FourCC(magic) != hecl)
                    continue;
                return ProjectRootPath(testPath);
            }
        }

        while (begin != end && *(end-1) != _S('/') && *(end-1) != _S('\\'))
            --end;
        if (begin != end)
            --end;
    }
    return ProjectRootPath();
}

ProjectRootPath SearchForProject(const SystemString& path, SystemString& subpathOut)
{
    ProjectRootPath testRoot(path);
    SystemString::const_iterator begin = testRoot.getAbsolutePath().begin();
    SystemString::const_iterator end = testRoot.getAbsolutePath().end();
    while (begin != end)
    {
        SystemString testPath(begin, end);
        SystemString testIndexPath = testPath + _S("/.hecl/beacon");
        Sstat theStat;
        if (!hecl::Stat(testIndexPath.c_str(), &theStat))
        {
            if (S_ISREG(theStat.st_mode))
            {
                FILE* fp = hecl::Fopen(testIndexPath.c_str(), _S("rb"));
                if (!fp)
                    continue;
                char magic[4];
                size_t readSize = fread(magic, 1, 4, fp);
                fclose(fp);
                if (readSize != 4)
                    continue;
                if (hecl::FourCC(magic) != FOURCC('HECL'))
                    continue;
                ProjectRootPath newRootPath = ProjectRootPath(testPath);
                SystemString::const_iterator origEnd = testRoot.getAbsolutePath().end();
                while (end != origEnd && *end != _S('/') && *end != _S('\\'))
                    ++end;
                subpathOut.assign(end, origEnd);
                return newRootPath;
            }
        }

        while (begin != end && *(end-1) != _S('/') && *(end-1) != _S('\\'))
            --end;
        if (begin != end)
            --end;
    }
    return ProjectRootPath();
}

}
