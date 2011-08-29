/* replacement of minput_list, which is available in m17n-lib 1.6.2+ (CVS) */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <m17n.h>

MPlist *
minput_list (MSymbol language)
{
    MPlist *imlist;
    MPlist *elm;
    MPlist *plist;

    plist = mplist ();
    imlist = mdatabase_list(msymbol("input-method"), language, Mnil, Mnil);
    for (elm = imlist; elm && mplist_key(elm) != Mnil; elm = mplist_next(elm)) {
        MDatabase *mdb = (MDatabase *) mplist_value(elm);
        MSymbol *tag = mdatabase_tag(mdb);
        MSymbol lang = tag[1];
        MSymbol name = tag[2];
        MPlist *l;

        l = mplist ();
        mplist_add (l, Msymbol, lang);
        mplist_add (l, Msymbol, name);
        if (tag[1] != Mnil && tag[2] != Mnil) {
            mplist_add (l, Msymbol, Mt);
        } else {
            mplist_add (l, Msymbol, Mnil);
        }
        mplist_add (plist, Mplist, l);
        m17n_object_unref (l);
    }
    if (imlist)
        m17n_object_unref (imlist);
    return plist;
}
