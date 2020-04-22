﻿using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.ComponentModel;

namespace golddrive
{
    [JsonObject(MemberSerialization.OptIn)]
    public class Drive : Observable
    {
        public Drive()
        {

        }
        public Drive(Drive d)
        {
            Clone(d);
        }
        public void Clone(Drive d)
        {
            Host = d.Host;
            Port = d.Port;
            User = d.User;
            Letter = d.Letter;
            Label = d.Label;
            Args = d.Args;
        }
        public DriveStatus Status { get; set; }

        public bool? IsGoldDrive { get; set; }

        public string ServerOS { get; set; }

        public string Path { get; set; }

        private bool _isDirty;
        public bool IsDirty
        {
            get { return _isDirty; }
            set
            {
                if (_isDirty != value)
                {
                    _isDirty = value;
                    NotifyPropertyChanged();
                }
            }
        }
        private string _host;
        public string Host
        {
            get { return _host; }
            set
            {
                if (_host != value)
                {
                    IsDirty = true;
                    _host = value;
                    NotifyPropertyChanged();
                }
                
            }
        }
        public string Letter { get; set; }

        public string Name { get { return $"{ Letter }:"; } }

        private string _args;

        [JsonProperty]
        public string Args
        {
            get { return string.IsNullOrEmpty(_args) ? null : _args; }
            set
            {
                if (_args != value)
                {
                    IsDirty = true;
                    _args = value;
                    NotifyPropertyChanged();
                }
            }
        }

        //private List<string> _hosts;
        //[JsonProperty]
        //public List<string> Hosts
        //{
        //    get
        //    {
        //        return _hosts ?? new List<string>();
        //    }
        //    set
        //    {
        //        _hosts = value;
        //    }
        //}

        private string _label;
        [JsonProperty]
        public string Label
        {
            get { return _label; }
            set
            {
                if (_label != value)
                {
                    IsDirty = true;
                    _label = value;

                    NotifyPropertyChanged();
                    NotifyPropertyChanged("ComboDisplay");
                }
            }
        }

        [JsonProperty]
        public string MountPoint
        {
            get 
            {
                string s = Host;
                if (string.IsNullOrEmpty(s))
                    return s;
                if (Port != "22")
                    s = string.Format($"{Host}!{Port}");
                if (User != Environment.UserName)
                    s = string.Format($"{User}@{s}");
                if (!string.IsNullOrEmpty(Path))
                    s = string.Format($"{s}{Path}");
                return s;  
            }
            set
            {
                string s = value;
                Host = s;
                if (string.IsNullOrEmpty(s))
                    return;

                
                if (s.Contains("\\"))
                {
                    Host = s.Split('\\')[0];
                    Path = s.Substring(s.IndexOf("\\")).Replace("\\", "/");
                    s = Host;
                    NotifyPropertyChanged("Path");
                }
                if (s.Contains("!"))
                {
                    Host = s.Split('!')[0];
                    int.TryParse(s.Split('!')[1], out int p);
                    if (p > 0)
                        Port = p.ToString();
                    s = Host;
                    NotifyPropertyChanged("Port");
                }
                if (s.Contains("@"))
                {
                    User = s.Split('@')[0];
                    Host = s.Split('@')[1];
                    NotifyPropertyChanged("User");
                }
                NotifyPropertyChanged();
                NotifyPropertyChanged("Host");
            }

        }

        private string user;
        public string User
        {
            get { return user == null ? Environment.UserName.ToLower() : user; }
            set {

                if (user != value)
                {
                    IsDirty = true;
                    user = value;
                    NotifyPropertyChanged();
                }
            }
        }

        private string port;
        public string Port
        {
            get { return port == null ? "22" : port; }
            set {
                if (port != value)
                {
                    IsDirty = true;
                    port = value;
                    NotifyPropertyChanged();
                }

            }
        }

        public string UserProfile
        {
            get
            {
                var prof = Environment.ExpandEnvironmentVariables("%USERPROFILE%");
                //if(!prof.Contains(User))
                //    prof = Regex.Replace(prof, @"(?i)([a-z]:\\Users\\)([a-z0-9_\.]+)", $@"$1{User}");
                return prof;
            }
        }
        public string AppKey
        {
            get { return $@"{UserProfile}\.ssh\id_golddrive_{User}"; }
            //get { return $@"{UserProfile}\.ssh\id_rsa"; }
        }
        //public string UserKey
        //{
        //    get { return $@"{UserProfile}\.ssh\id_rsa"; }
        //}
        public string AppPubKey
        {
            get { return $@"{AppKey}.pub"; }
        }

        public string RegistryMountPoint2
        {
            get { return Remote?.Replace("\\", "#"); }
        }
        public string Remote
        {
            get { return $@"\\golddrive\{MountPoint}"; }
        }
        public string ComboDisplay
        {
            get
            {
                //int maxLengh = 15;
                string display = $"{ Name } {Label}";
                return display;

                //if (string.IsNullOrEmpty(s))
                //    s = Remote;
                //if (s.Length > maxLengh)
                //    s = "..." + s.Substring(s.Length - maxLengh);
                //return $"{ Letter }: {s}";
            }
        }

        public void Trim()
        {
            Host = Host?.Trim();
            Port = Port?.Trim();
            Label = Label?.Trim();
            User = User?.Trim();
            User = User?.ToLower();
            Args = Args?.Trim();
            if (Port == "22")
                Port = "";
            if (User == Environment.UserName.ToLower())
                User = "";
        }

        public override string ToString()
        {
            return $"{ Name } {MountPoint}"; ;
        }
    }

    public class DriveList : List<Drive>
    {
        public DriveList() { }
    }
}
