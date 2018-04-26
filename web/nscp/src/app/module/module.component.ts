import { Component, OnInit } from '@angular/core';
import { ModuleService } from '../services/module.service';
import { QueryService } from '../services/query.service';
import { Module } from '../services/module';
import { Query } from '../services/query';
import { MatSnackBar, MatSlideToggleChange } from '@angular/material';
import { map } from 'rxjs/operators';
import { ActivatedRoute } from '@angular/router';

class ModuleUI implements Module {
  id: string;
  title: string;
  name: string;
  description: string;
  loaded: boolean;
  enabled: boolean;
  shortDescription: string;
  metadata: Map<string,string>;
  filterString: String;
  queries : Query[];

  constructor(m: Module) {
    this.id = m.id;
    this.title = m.title;
    this.name = m.name;
    this.description = m.description;
    this.loaded = m.loaded;
    this.enabled = m.enabled;
    this.metadata = m.metadata;
    this.filterString = this.title.toLowerCase() + ", " + this.name.toLowerCase() + ", " + this.description.toLowerCase();
    if (this.description.length > 50) {
      this.shortDescription = this.description.substring(0, 50) + "...";
    } else {
      this.shortDescription = this.description;
    }
  }
}


@Component({
  selector: 'app-module',
  templateUrl: './module.component.html',
  styleUrls: ['./module.component.scss'],
  providers: [ModuleService, QueryService]
})
export class ModuleComponent implements OnInit {

  modules : ModuleUI[];
  filteredModules : ModuleUI[];
  selected : ModuleUI;
  selectedModule: string;
  queries : Query[];

  constructor(private moduleService: ModuleService,
    private queryService: QueryService,
    public snackBar: MatSnackBar,
    private route: ActivatedRoute) {}

  ngOnInit() {
    this.route.params.subscribe(params => {
      if (params['id']) {
        this.selectModule(params['id']);
      }
   });
    this.moduleService.list()
      .pipe(
        map(list => list.map(q => new ModuleUI(q)))
      )
      .subscribe( list => {
        this.modules = list;
        this.filteredModules = list;
        if (!this.selectedModule && this.filteredModules.length > 0) {
          this.selectedModule = this.filteredModules[0].name;
        }
        this.selectModule(this.selectedModule);
      });
  }

  selectModule(id: string) : void {
    this.selectedModule = id;
    if (this.modules) {
      this.selected = this.modules.find(m => m.name == this.selectedModule);
    }

    if (this.selected) {
      this.queryService.list()
      .subscribe( list => {
        this.queries = list;
        this.selected.queries = this.queries.filter(q => q.plugin == this.selectedModule);
      });
    }


  }

  /*select(m: ModuleUI) : void {
    this.selected = m;
  }*/
  toggle(m: ModuleUI, event: MatSlideToggleChange) : void {
    if (m.enabled) {
      //event.checked = true;
      this.disable(m);
    } else {
      //event.checked = false;
      this.enable(m);
      this.load(m);
    }
  }
  load(m: ModuleUI) : void {
    this.moduleService.load(m.name).subscribe(
        (msg) => m.loaded = true,
        (err) => this.failure(`Failed to load ${m.name}: ${err}`)
    );
  }
  unload(m: ModuleUI) : void{
    this.moduleService.unload(m.name)
    .subscribe(
      (msg) => m.loaded = false,
      (err) => this.failure(`Failed to unload ${m.name}: ${err}`)
    );
  }
  enable(m: ModuleUI) : void {
    this.moduleService.enable(m.name).subscribe(
        (msg) => m.enabled = true,
        (err) => this.failure(`Failed to enable ${m.name}: ${err}`)
    );
  }
  disable(m: ModuleUI) : void{
    this.moduleService.disable(m.name)
    .subscribe(
      (msg) => m.enabled = false,
      (err) => this.failure(`Failed to disable ${m.name}: ${err}`)
    );
  }

  failure(message: string) {
    this.snackBar.open(message, "Ok", {
      duration: 2000,
    });    
  }

  filterChanged(searchText: string) {
    let data = searchText.toLowerCase();
    this.filteredModules = this.modules.filter(item => item.filterString.indexOf(data) > -1);
  }
}
