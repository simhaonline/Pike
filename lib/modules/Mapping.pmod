#pike __REAL_VERSION__
#pragma strict_types

//! Alias for @[m_delete()]
constant delete=m_delete;

constant Iterator = __builtin.mapping_iterator;

//! A mapping look-alike that overrides (ie shadows) another @[parent] mapping.
//!
//! The class implements most of the usual mapping operations.
class ShadowedMapping(protected mapping|ShadowedMapping parent)
{
  protected mapping shadow = ([]);

  protected mapping joined;
  protected mapping parent_copy;

  protected int(0..1) modify_parent;

  //! @decl void create(mapping|ShadowedMapping parent, @
  //!                   mapping|void shadow, int(0..1)|void modify_parent)
  //!
  //! @param parent
  //!   Mapping to be shadowed.
  //! @param shadow
  //!   Initial shadow of @[parent].
  //! @param modify_parent
  //!   Modifications should be done to @[parent] rather than
  //!   to @[shadow]. If this is set, only entries that are
  //!   already present in @[shadow] can be modified by later
  //!   operations.

  protected void create(mapping|void shadow, int(0..1)|void modify_parent)
  {
    if (shadow) this_program::shadow = shadow + ([]);
    this_program::modify_parent = modify_parent;
  }

  // Updates the cached joined mapping if needed.
  protected void update_joined()
  {
    if (!joined || !equal(parent, parent_copy)) {
      joined = [mapping](parent + shadow);
      parent_copy = [mapping](parent + ([]));
    }
  }

  protected mixed `[](mixed ind)
  {
    mixed res = shadow[ind];
    if (!undefinedp(res)) return res;
    return parent[ind];
  }

  protected void `[]=(mixed ind, mixed val)
  {
    joined = 0;
    if (modify_parent && !has_index(shadow, ind)) {
      parent[ind] = val;
    } else {
      shadow[ind] = val;
    }
  }

  protected mixed `->(string ind)
  {
    return `[](ind);
  }

  protected void `->=(string ind, mixed val)
  {
    `[]=(ind, val);
  }

  protected int(0..1) _equal(mixed other)
  {
    update_joined();
    return equal(other, joined);
  }

  protected mixed _m_delete(mixed ind)
  {
    mixed res = m_delete(shadow, ind);
    if (undefinedp(res))
      res = m_delete(parent, ind);
    else
      joined = 0;

    return res;
  }

  protected array(mixed) _indices()
  {
    update_joined();
    return indices(joined);
  }

  protected array(mixed) _values()
  {
    update_joined();
    return values(joined);
  }

  protected mixed `+(mixed ... args)
  {
    update_joined();
    return predef::`+(joined, @args);
  }

  protected mixed ``+(mixed ... args)
  {
    update_joined();
    return predef::`+(@args, joined);
  }

  protected mixed `-(mixed ... args)
  {
    update_joined();
    return predef::`-(joined, @args);
  }

  protected mixed ``-(mixed ... args)
  {
    update_joined();
    return predef::`-(@args, joined);
  }

  protected mixed `|(mixed ... args)
  {
    update_joined();
    return predef::`|(joined, @args);
  }

  protected mixed ``|(mixed ... args)
  {
    update_joined();
    return predef::`|(@args, joined);
  }

  protected mixed `&(mixed ... args)
  {
    update_joined();
    return predef::`&(joined, @args);
  }

  protected mixed ``&(mixed ... args)
  {
    update_joined();
    return predef::`&(@args, joined);
  }

  protected mixed `^(mixed ... args)
  {
    update_joined();
    return predef::`^(joined, @args);
  }

  protected mixed ``^(mixed ... args)
  {
    update_joined();
    return predef::`^(@args, joined);
  }

  protected mixed cast(string type)
  {
    update_joined();
    switch(type) {
    case "mapping": return joined + ([]);
    case "array": return (array)joined;
    }
    return UNDEFINED;
  }

  protected mixed _search(mixed val)
  {
    update_joined();
    return search(joined, val);
  }

  protected string _sprintf(int c)
  {
    update_joined();
    return sprintf(sprintf("%%%c", c), joined);
  }
}
