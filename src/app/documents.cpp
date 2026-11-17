#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>

#include <app/documents.h>
#include <app/session.h>
#include <core/toolchain.h>

namespace {

const size_t maxRecentFiles = 10;

/// @return The recent files list, one path per line.
std::filesystem::path recentFilesPath() {
    return configDirectory() / "recent_files";
}

/// @brief Rereads a C document's header declarations unless its path and preprocessor lines
///        are unchanged; other documents get none.
void readDeclarations( Document &document, const std::string &text ) {
    bool const isC = !document.path.empty() && sourceLanguage( document.path ) == SourceLanguage::C;
    std::string const directives = isC ? preprocessorLines( text ) : "";
    std::string const source = isC ? document.path + '\n' + directives : "";
    if ( source == document.declarationsSource ) {
        return;
    }
    document.declarationsSource = source;
    document.declarations.clear();
    if ( isC ) {
        document.declarations =
            headerDeclarations( directives, std::filesystem::path( document.path ).parent_path() );
    }
}

} // anonymous namespace

bool isModified( const Document &document ) {
    return document.savedVersion && document.version != *document.savedVersion;
}

std::string documentName( const std::string &path ) {
    return path.empty() ? "untitled" : std::filesystem::path( path ).filename().string();
}

std::string displayPath( const std::string &path ) {
    const char *home = std::getenv( "HOME" );
    if ( home && path.starts_with( std::string( home ) + "/" ) ) {
        return "~" + path.substr( std::strlen( home ) );
    }
    return path;
}

std::string absolutePath( const std::string &path ) {
    return std::filesystem::absolute( path ).lexically_normal();
}

std::filesystem::path configDirectory() {
    const char *configHome = std::getenv( "XDG_CONFIG_HOME" );
    const char *home = std::getenv( "HOME" );
    std::filesystem::path directory =
        configHome ? configHome : std::string( home ? home : "." ) + "/.config";
    directory /= "arm_ide";
    std::error_code ignored;
    std::filesystem::create_directories( directory, ignored );
    return directory;
}

std::vector<std::string> readRecentFiles() {
    std::vector<std::string> paths;
    std::ifstream file( recentFilesPath() );
    std::string path;
    while ( std::getline( file, path ) ) {
        if ( !path.empty() && std::filesystem::exists( path ) ) {
            paths.push_back( path );
        }
    }
    return paths;
}

void writeRecentFiles( const std::vector<std::string> &paths ) {
    std::ofstream file( recentFilesPath() );
    for ( const std::string &path : paths ) {
        file << path << '\n';
    }
}

// Session's document methods

const std::vector<Document> &Session::documents() const {
    return openDocuments;
}

const Document &Session::activeDocument() const {
    for ( const Document &document : openDocuments ) {
        if ( document.id == activeDocumentId ) {
            return document;
        }
    }
    return openDocuments.front();
}

bool Session::openFile( const std::string &path ) {
    std::string const fullPath = absolutePath( path );
    for ( const Document &document : openDocuments ) {
        if ( document.path == fullPath ) {
            activeDocumentId = document.id;
            return true;
        }
    }

    if ( !std::filesystem::exists( fullPath ) ) {
        status = "File no longer exists: " + path;
        removeRecentFile( fullPath );
        return false;
    }
    std::ifstream const file( fullPath );
    if ( !file ) {
        status = "Cannot open " + path;
        return false;
    }
    std::stringstream content;
    content << file.rdbuf();

    const Document &active = activeDocument();
    bool const replacesEmptyUntitled = active.path.empty() && !isModified( active );
    int const replacedId = active.id;
    activeDocumentId = addDocument( fullPath, content.str() );
    if ( replacesEmptyUntitled ) {
        closeDocument( replacedId );
    }
    status = "Opened " + path;
    addRecentFile( fullPath );
    return true;
}

void Session::activateDocument( int id ) {
    if ( findDocument( id ) ) {
        activeDocumentId = id;
    }
}

void Session::closeDocument( int id ) {
    auto position = std::ranges::find( openDocuments, id, &Document::id );
    if ( position == openDocuments.end() ) {
        return;
    }
    size_t const index = position - openDocuments.begin();
    openDocuments.erase( position );
    if ( openDocuments.empty() ) {
        addDocument( "", "" );
    }
    if ( activeDocumentId == id ) {
        activeDocumentId = openDocuments[std::min( index, openDocuments.size() - 1 )].id;
    }
}

void Session::setDocumentVersion( int id, size_t version ) {
    if ( Document *document = findDocument( id ) ) {
        document->version = version;
        if ( !document->savedVersion ) {
            document->savedVersion = version;
        }
    }
}

void Session::updateDeclarations( int id, const std::string &text ) {
    if ( Document *document = findDocument( id ) ) {
        readDeclarations( *document, text );
    }
}

std::vector<std::string> Session::unsavedDocumentNames() const {
    std::vector<std::string> names;
    for ( const Document &document : openDocuments ) {
        if ( isModified( document ) ) {
            names.push_back( documentName( document.path ) );
        }
    }
    return names;
}

void Session::showProgramLine( int line ) {
    for ( const Document &document : openDocuments ) {
        if ( !document.path.empty() && document.path == programFilePath ) {
            activeDocumentId = document.id;
            lineToReveal = line;
        }
    }
}

int Session::takeLineToReveal() {
    return std::exchange( lineToReveal, 0 );
}

Document *Session::findDocument( int id ) {
    auto position = std::ranges::find( openDocuments, id, &Document::id );
    return position == openDocuments.end() ? nullptr : &*position;
}

Document &Session::activeDocumentForEdit() {
    return *findDocument( activeDocument().id );
}

int Session::addDocument( const std::string &path, const std::string &text ) {
    Document document;
    document.id = nextDocumentId++;
    document.path = path;
    document.diskText = text;
    readDeclarations( document, text );
    openDocuments.push_back( document );
    if ( activeDocumentId == 0 ) {
        activeDocumentId = document.id;
    }
    return document.id;
}

/// @brief Breakpoints follow the file to a new name.
bool Session::saveActiveDocument( const std::string &path, const CommandInput &input ) {
    std::ofstream file( path );
    file << input.text;
    if ( !file ) {
        status = "Cannot save " + path;
        return false;
    }

    Document &document = activeDocumentForEdit();
    std::string const savedPath = absolutePath( path );
    if ( savedPath != document.path ) {
        breakpoints[savedPath] = std::move( breakpoints[document.path] );
        breakpoints.erase( document.path );
    }
    document.path = savedPath;
    document.diskText = input.text;
    readDeclarations( document, input.text );
    document.version = input.version;
    document.savedVersion = input.version;
    status = "Saved " + path;
    addRecentFile( savedPath );
    return true;
}

bool Session::saveBeforeBuild( const CommandInput &input ) {
    return saveActiveDocument( input.path.empty() ? activeDocument().path : input.path, input );
}

void Session::addRecentFile( const std::string &path ) {
    std::erase( recentFiles, path );
    recentFiles.insert( recentFiles.begin(), path );
    if ( recentFiles.size() > maxRecentFiles ) {
        recentFiles.resize( maxRecentFiles );
    }
    writeRecentFiles( recentFiles );
}

void Session::removeRecentFile( const std::string &path ) {
    std::erase( recentFiles, path );
    writeRecentFiles( recentFiles );
}
