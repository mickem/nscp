import { Component, OnInit } from '@angular/core';
import { SettingsKey, Settings, SettingsDesc } from '../services/settings';
import { SettingsService } from '../services/settings.service';
import { MatSnackBar } from '@angular/material';
import { ActivatedRoute } from '@angular/router';
import { map } from 'rxjs/operators';


class SettingsDescUI implements SettingsDesc {
  default_value: string;
  description: string;
  icon: string;
  is_advanced_key: boolean;
  is_object: boolean;
  is_sample_key: string;
  is_template_key: string;
  key: string;
  path: string;
  plugins: string;
  sample_usage: string;
  title: string;
  value: string;
  original_value: string;

  filterString: string;
  isBundle: boolean;

  constructor(key: SettingsDesc) {
    this.title = key.title;
    this.description = key.description;
    this.default_value = key.default_value;
    this.icon = key.icon;
    this.is_advanced_key = key.is_advanced_key;
    this.is_object = key.is_object;
    this.is_sample_key = key.is_sample_key;
    this.is_template_key = key.is_template_key;
    this.key = key.key;
    this.path = key.path;
    this.plugins = key.plugins;
    this.sample_usage = key.sample_usage;
    this.value = key.value;
    this.original_value = key.value;
    this.isBundle = false;
    this.filterString = "title:" + this.title.toLowerCase() + ", path:" + this.path.toLowerCase() + ", key:" + this.key.toLowerCase();;
    for (let p of this.plugins) {
      this.filterString = this.filterString + ", module:" + p.toLowerCase();
    }
  }
}


class Node {
  title: string;
  path: string;
  icon: string;
  hidden: boolean;
  filterString: string;
  settings: SettingsDescUI[] = [];
  filteredSettings : SettingsDescUI[] = [];
  samples : SettingsDescUI[] = [];

  constructor(key: string, path:string, icon:string) {
    this.title = key;
    this.path = path;
    this.icon = icon;
    this.hidden = false;
  }

  append(key: SettingsDescUI) : void {
    this.settings.push(key);
    this.filteredSettings.push(key);
  }

  filter(hideAdvancedKeys: boolean, hideUnchanged: boolean, searchText: string) : void {
    this.filteredSettings = this.settings.filter(item => item.filterString.indexOf(searchText) > -1);
    if (hideAdvancedKeys) {
      this.filteredSettings = this.filteredSettings.filter(item => item.is_advanced_key == false);
    }
    if (hideUnchanged) {
      this.filteredSettings = this.filteredSettings.filter(item => item.value != item.default_value);
    }
  }

  isVisible() : boolean {
    if (this.hidden) {
      return false;
    }
    return this.filteredSettings.length > 0;
  }

  find(path:string, key: string) : SettingsDescUI[] {
    if (key == "*") {
      return this.settings.filter(k => k.path == path);
    }
    return [this.settings.find(k => k.path == path && k.key == key)];
  }
  has(path:string, key: string) : boolean {
    return this.find(path, key).length > 0;
  }
  
} 
function push(map: Map<String,Node>, key: string, nodePath: string, nodeIcon: string, node: SettingsDescUI) {
  if (!( key in map)) {
    map[key] = new Node(key, nodePath, nodeIcon);
  }
  map[key].append(node);
}
function getIcon(path:string) : string{
  if (path == "/modules") {
    return "cube";
  }
  if (path == "/paths") {
    return "folder"
  }
  if (path == "/settings/WEB/server") {
    return "cloud"
  }
  if (path == "/settings/crash") {
    return "bug";
  }
  if (path == "/settings/log" || path == "/settings/log/file") {
    return "file-alt";
  }
  if (path == "/includes") {
    return "file-alt";
  }
  if (path.startsWith("/settings/WEB/server/users") || path.startsWith("/settings/WEB/server/roles")) {
    return "user";
  }
  if (path == "/settings/core" || path == "/settings/default") {
    return "cogs";
  }
  if (path == "/settings/eventlog" || path.startsWith("/settings/eventlog/real-time")) {
    return "file-signature";

  }
  console.log(path);
  return "question";
}

function buildListBySection(settingsKeys: SettingsDescUI[]) : Node[] {
  var objects = [];
  var sorted = new Map<String,Node>();
  var paths = new Map<String,Node>();
  var samples = new Map<String,Node>();
  for (let k of settingsKeys) {
    if (k.is_sample_key) {
      push(samples, k.path, k.path, "", k);
      //console.log("sample 1:", k);
      continue;
    }
   if (k.is_object) {
      objects.push(k.path);
    }
    let title = k.title;
    if (!k.title) {
      title = k.path;
    }
    paths[k.path] = k;
   }
   console.log(samples);
   for (let k of settingsKeys) {
     if (k.is_sample_key) {
       continue;
     }
    let key = k.path;
    if (k.key) {
      let isObj = false;
      for (let p of objects) {
        if (k.path.startsWith(p)) {
          isObj = true;
          continue;
        }
      }
      push(sorted, key, k.path, getIcon(k.path), k);
      if (isObj) {
        sorted[key].hidden = true;
      }
    } else {
      let title = k.title;
      if (!title) {
        title = k.path;
      }
      let obj = false;
      for (let p of objects) {
        if (k.path.startsWith(p)) {
          if (k.path == p) {
            key = p;
          } else {
            // THis is a bundle, so wrap it up and add "new"
            k.title = k.path.substr(p.length+1);
            k.isBundle = true;
            k.key = "*";
            push(sorted, p, k.path,  getIcon(p), k);
            if (samples[p + "/sample"]) {
              sorted[p].samples = samples[p + "/sample"].filteredSettings;
            } else {
              console.log(p);
            }
            console.log(p, sorted[p].samples, samples);
            obj = true;
          }
        }
      }
      if (obj) {
        //continue;
      }
      if (!( key in sorted)) {
        sorted[key] = new Node(title, k.path, getIcon(k.path));
      } else {
        sorted[key].title = title;
        if (obj) {
          sorted[key].hidden = true;
        }
      }
    }
   }
  return Object.values(sorted);
}

@Component({
  selector: 'app-settings',
  templateUrl: './settings.component.html',
  styleUrls: ['./settings.component.css']
})
export class SettingsComponent implements OnInit {

  selected : SettingsDescUI[];
  selectedPath: string;
  selectedKey: string;
  filterText: string;
  settings: Node[];
  hideAdvancedKeys : boolean = true;
  hideUnchanged : boolean = false;

  constructor(private settingsService: SettingsService,
    public snackBar: MatSnackBar,
    private route: ActivatedRoute) {}

  ngOnInit() {
    this.route.params.subscribe(params => {
      if (params['path'] && params['key']) {
        this.selectKey(params['path'], params['key']);
      } else if (params['path']) {
        this.filterChanged(params['path']);
      }
   });
    this.settingsService.desc("/")
      .pipe(
        map(list => list.map(q => new SettingsDescUI(q)))
      )
      .subscribe(modules => {
        this.settings = buildListBySection(modules);
        if (this.selectedPath && this.selectedKey) {
          this.selectKey(this.selectedPath, this.selectedKey);
        } else if (this.filterText) {
          this.filterChanged(this.filterText);
        }
        this.applyFilters();
      });
  }

  toggleAdvancedKeys() {
    this.hideAdvancedKeys = !this.hideAdvancedKeys;
    this.applyFilters();
  }

  toggleUnchanged() {
    this.hideUnchanged = !this.hideUnchanged;
    this.applyFilters();
  }

  applyFilters() {
    if (this.filterText) {
      this.filterChanged(this.filterText);
    } else {
      if (this.settings) {
        for (let m of this.settings) {
          m.filter(this.hideAdvancedKeys, this.hideUnchanged, "");
        }
      }
    }
  }

  selectKey(path: string, key: string) : void {
    this.selectedPath = path;
    this.selectedKey = key;
    if (this.settings) {
      for (let node of this.settings) {
        let s = node.find(path, key);
        if (s.length > 0) {
          if (s.filter(k => k && !k.isBundle).length > 0) {
            this.selected = s;
            return;
          }
        }
      }
    }
  }

  filterChanged(searchText: string) {
    this.filterText = searchText;
    let data = searchText.toLowerCase();
    if (this.settings) {
      for (let m of this.settings) {
        m.filter(this.hideAdvancedKeys, this.hideUnchanged, data);
      }
    }
  }
}
