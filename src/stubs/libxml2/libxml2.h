typedef enum {
    XMLDOC,
    XMLTAG,
    SVGROOT,
    SVG_DEFS,
    SVG_USE,
    SVG_LINEAR_GRADIENT,
    SVG_RADIAL_GRADIENT,
    SVG_G,
    SVG_STYLE,
    SVG_PATH,
    SVG_RECT,
    SVG_CIRCLE,
    SVG_ELLIPSE
} SVGNodeType;

typedef enum {
    XML_ELEMENT_NODE=		1,
    XML_ATTRIBUTE_NODE=		2,
    XML_TEXT_NODE=		3,
    XML_CDATA_SECTION_NODE=	4,
    XML_ENTITY_REF_NODE=	5,
    XML_ENTITY_NODE=		6,  /* unused */
    XML_PI_NODE=		7,
    XML_COMMENT_NODE=		8,
    XML_DOCUMENT_NODE=		9,
    XML_DOCUMENT_TYPE_NODE=	10, /* unused */
    XML_DOCUMENT_FRAG_NODE=	11,
    XML_NOTATION_NODE=		12, /* unused */
    XML_HTML_DOCUMENT_NODE=	13,
    XML_DTD_NODE=		14,
    XML_ELEMENT_DECL=		15,
    XML_ATTRIBUTE_DECL=		16,
    XML_ENTITY_DECL=		17,
    XML_NAMESPACE_DECL=		18,
    XML_XINCLUDE_START=		19,
    XML_XINCLUDE_END=		20
    /* XML_DOCB_DOCUMENT_NODE=	21 */ /* removed */
} xmlElementType;
typedef char xmlChar;

struct _xmlNode {
    void           *_private;	/* application data */
    xmlElementType   type;	/* type number, must be second ! */
    const xmlChar   *name;      /* the name of the node, or the entity */
    struct _xmlNode *children;	/* parent->childs link */
    struct _xmlNode *last;	/* last child link */
    struct _xmlNode *parent;	/* child->parent link */
    struct _xmlNode *next;	/* next sibling link  */
    struct _xmlNode *prev;	/* previous sibling link  */
    struct _xmlDoc  *doc;	/* the containing document */

    /* End of common part */
    void           *ns;        /* pointer to the associated namespace */
    xmlChar         *content;   /* the content */
    void *properties;/* properties list */
    void           *nsDef;     /* namespace definitions on this node */
    void            *psvi;	/* for type/PSVI information */
    unsigned short   line;	/* line number */
    unsigned short   extra;	/* extra data for XPath/XSLT */
};
typedef struct _xmlNode xmlNode;
typedef void xmlDoc;

xmlChar* xmlGetProp(xmlNode* node, const xmlChar* name);
int xmlStrcmp(const xmlChar* str1, const xmlChar* str2);
xmlDoc* xmlParseDoc(const xmlChar* buffer);
xmlNode* xmlDocGetRootElement(xmlDoc* doc);
void xmlFreeDoc(xmlDoc* doc);
xmlChar* xmlNodeGetContent(xmlNode* node);
void xmlCleanupParser();
