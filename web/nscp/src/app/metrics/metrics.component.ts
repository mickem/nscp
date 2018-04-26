import { Component, OnInit } from '@angular/core';
import { MetricsService } from '../services/metrics.service';
import { map } from 'rxjs/operators';
import { MatTableDataSource } from '@angular/material';

class Metric {
  key: string;
  value: string;

  constructor(key: string, value: string) {
    this.key = key;
    this.value = value;
  }
}

function makeTable(metrics: Map<string, string>) : Metric[] {
  let ret = new Array<Metric>();

  for (let key in metrics) {
    ret.push(new Metric(key, metrics[key]));
  }
  return ret;
}


@Component({
  selector: 'app-metrics',
  templateUrl: './metrics.component.html',
  styleUrls: ['./metrics.component.css']
})
export class MetricsComponent implements OnInit {

  metrics: Map<string, string>;
  displayedColumns = ['key', 'value'];
  dataSource = new MatTableDataSource();
  isLoadingResults = true;

  constructor(private metricsService: MetricsService) { }

  ngOnInit() {
    this.refresh();
  }

  refresh() {
    this.isLoadingResults = true;
    this.metricsService.list()
      .subscribe( list => {
        this.dataSource.data = makeTable(list);
        this.isLoadingResults = false;
      });
  }


}
