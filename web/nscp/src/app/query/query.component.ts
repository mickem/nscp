import { Component, OnInit } from '@angular/core';
import { Observable } from 'rxjs';
import { QueryService } from '../services/query.service';
import { Query, FullQuery, QueryResponse } from '../services/query';
import { map } from 'rxjs/operators';
import { ActivatedRoute } from '@angular/router';

const STATUSES = {
  0: 'ok',
  1: 'warning',
  2: 'critical',
  3: 'unknown'
}
const STATUS_COLORS = {
  0: '#4CAF50',
  1: '#FFEB3B',
  2: '#F44336',
  3: '#E91E63'
}



class QueryUI implements Query {
  title: string;
  name: string;
  description: string;
  shortDescription: string;
  metadata: Map<string,string>;
  hasFull: boolean;
  panelOpenState: boolean;
  command: string;
  result: QueryResponse;
  resultText: string;
  resultColor: string;
  resultStatus: string;
  filterString: string;
  plugin: string;

  constructor(q: Query) {
    this.title = q.title;
    this.name = q.name;
    this.description = q.description;
    this.metadata = q.metadata;
    this.plugin = q.plugin || "";
    this.hasFull = false;
    this.panelOpenState = false;
    this.command = this.name;
    this.result = null;
    this.filterString = this.title.toLowerCase() + ", " + this.name.toLowerCase() + ", " + this.description.toLowerCase() + ", " + this.plugin.toLowerCase();

    if (this.description.length > 50) {
      this.shortDescription = this.description.substring(0, 50) + "...";
    } else {
      this.shortDescription = this.description;
    }
  }

  get(queryService: QueryService) : void {
    if (this.hasFull) {
      return;
    }
    queryService.get(this.name)
    .subscribe(fullQuery => {
      this.metadata = fullQuery.metadata; 
      this.hasFull = true
      this.plugin = fullQuery.plugin;
      this.filterString = this.title.toLowerCase() + ", " + this.name.toLowerCase() + ", " + this.description.toLowerCase() + ", " + this.plugin.toLowerCase();
    });
  }

  execute(queryService: QueryService) : void {
    queryService.exec(this.command)
      .subscribe(result => this.setResult(result));
  }

  setResult(r: QueryResponse) : void {
    this.result = r;
    this.resultText = r.lines.map(l => l.message).join("\n");
    this.resultColor = STATUS_COLORS[r.result];
    this.resultStatus = STATUSES[r.result];
    console.log(this.resultText);
  }
}

@Component({
  selector: 'app-query',
  templateUrl: './query.component.html',
  styleUrls: ['./query.component.css'],
  providers: [QueryService]
})
export class QueryComponent implements OnInit {

  queries : QueryUI[];
  filteredQueries : QueryUI[];
  selected: QueryUI;
  selectedQuery: string;

  constructor(private queryService: QueryService,
    private route: ActivatedRoute) {}

  ngOnInit() {
    this.route.params.subscribe(params => {
      if (params['id']) {
        this.selectQuery(params['id']);
      }
    });
    
    this.queryService.list()
      .pipe(
        map(list => list.map(q => new QueryUI(q)))
      )
      .subscribe( list => {
        this.queries = list;
        this.filteredQueries = list;
        if (!this.selectedQuery && this.filteredQueries.length > 0) {
          this.selectedQuery = this.filteredQueries[0].name;
        }
        this.selectQuery(this.selectedQuery);
      });
  }

  selectQuery(query: string) : void {
    this.selectedQuery = query;
    if (this.queries) {
      this.selected = this.queries.find(m => m.name == this.selectedQuery);
    }
  }

  select(q: QueryUI) : void {
    q.get(this.queryService);
    this.selected = q;
  }
  execute(q: QueryUI) : void {
    q.execute(this.queryService);
  }

  open(q: QueryUI) : void {
    q.get(this.queryService);
    q.panelOpenState = true;
  }
  close(q: QueryUI) : void {
    q.panelOpenState = false;
  }
  filterChanged(searchText: string) {
    let data = searchText.toLowerCase();
    this.filteredQueries = this.queries.filter(item => item.filterString.indexOf(data) > -1);
  }

}
