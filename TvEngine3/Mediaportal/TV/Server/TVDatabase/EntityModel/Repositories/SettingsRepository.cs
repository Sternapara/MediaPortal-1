﻿using System;
using System.Collections;
using System.Collections.Generic;
using System.Data.Entity;
using System.Data.Objects;
using System.Linq;
using System.Linq.Expressions;
using System.Text;
using Mediaportal.TV.Server.TVDatabase.Entities;
using Mediaportal.TV.Server.TVDatabase.EntityModel.Interfaces;

namespace Mediaportal.TV.Server.TVDatabase.EntityModel.Repositories
{
  public class SettingsRepository : GenericRepository<Model>, ISettingsRepository
  {
    public SettingsRepository()    
    {
    }

    public SettingsRepository(Model context)
      : base(context)
    {
    }

    public SettingsRepository(bool trackingEnabled) : base(trackingEnabled)
    {      
    }

    /// <summary>
    /// saves a value to the database table "Setting"
    /// </summary>    
    public void SaveSetting(string tagName, string value)
    {            
      Setting setting = First<Setting>(s => s.tag == tagName);
      if (setting == null)
      {
        setting = new Setting { value = value, tag = tagName };
        Add(setting);
      }
      else
      {
        setting.value = value;
      }

      UnitOfWork.SaveChanges();
      setting.AcceptChanges();
    }

    public Setting GetOrSaveSetting(string tagName, string defaultValue)
    {
      if (defaultValue == null)
      {
        return null;
      }
      if (tagName == null)
      {
        return null;
      }
      if (tagName == "")
      {
        return null;
      }

      Setting setting = First<Setting>(s => s.tag == tagName);
      if (setting == null)
      {
        setting = new Setting {value = defaultValue, tag = tagName};
        Add(setting);
        UnitOfWork.SaveChanges();
      }
            
      return setting;
    }

    /// <summary>
    /// gets a value from the database table "Setting"
    /// </summary>
    /// <returns>A Setting object with the stored value, if it doesnt exist a empty string will be the value</returns>
    public Setting GetSetting(string tagName)
    {
      var setting = GetOrSaveSetting(tagName, "");
      return setting;
    }
  }
}
