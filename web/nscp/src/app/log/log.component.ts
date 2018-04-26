import { Component, OnInit, ViewChild, AfterViewInit } from '@angular/core';
import { LogRecord } from '../services/logrecord';
import { LogService } from '../services/log.service';
import { map, tap } from 'rxjs/operators';
import { HttpHeaders } from '@angular/common/http';
import { PageEvent, MatPaginator, MatTableDataSource } from '@angular/material';


class LogRecordUI implements LogRecord {
  level: string;
  message: string;
  date: Date;
  file: string;
  line: number;

  constructor(l: LogRecord) {
    this.level = l.level;
    this.message = l.message;
    this.date = l.date;
    this.file = l.file;
    this.line = l.line;
  }

}

/*
 * parse_link_header()
 *
 * Parse the Github Link HTTP header used for pageination
 * http://developer.github.com/v3/#pagination
 */
function parse_link_header(header) {
  if (header.length === 0) {
      throw new Error("input must not be of zero length");
  }

  // Split parts by comma
  var parts = header.split(',');
  var links = {};
  // Parse each part into a named link
  for(var i=0; i<parts.length; i++) {
      var section = parts[i].split(';');
      if (section.length !== 2) {
          throw new Error("section could not be split on ';'");
      }
      var url = section[0].replace(/<(.*)>/, '$1').trim();
      var name = section[1].replace(/rel="(.*)"/, '$1').trim();
      links[name] = url;
  }
  return links;
}

@Component({
  selector: 'app-log',
  templateUrl: './log.component.html',
  styleUrls: ['./log.component.css']
})
export class LogComponent implements AfterViewInit {

  logs : LogRecordUI[];
  paginationCount: number;
  paginationPage: number;
  paginationLimit: number;
  pageSizeOptions = [10, 25, 100];
  pageEvent: PageEvent;
  displayedColumns = ['date', 'level', 'message'];
  dataSource = new MatTableDataSource();
  @ViewChild(MatPaginator) paginator: MatPaginator;
  isLoadingResults = true;
  showColumns = {
    'date': true, 
    'level': true,
    'message': true,
    'file': false
  }
  
  constructor(private logService: LogService) {}

  ngOnInit() {
  }

  ngAfterViewInit() {
    this.refresh();
    this.paginator.page
      .pipe(
        tap(() => this.refresh())
      )
      .subscribe();
  }

  toggleColumn(column:string) {
    this.showColumns[column] = ! this.showColumns[column];
    let cols = [];
    for (let itm in this.showColumns) {
      if (this.showColumns[itm] == true) {
        cols.push(itm);
      }
    }
    this.displayedColumns = cols;
  }

  refresh() : void {
    this.isLoadingResults = true;
    this.logService.list(this.paginator.pageIndex+1, this.paginator.pageSize)
    .pipe(
      map(list => {
        this.paginator.length = +list.headers.get("X-Pagination-Count");
        this.paginator.pageIndex = (+list.headers.get("X-Pagination-Page"))-1;
        //this.paginator.pageSize = +list.headers.get("X-Pagination-Limit");
        return list.body.map(l => new LogRecordUI(l));
      })
    )
    .subscribe( list => {
      this.dataSource.data = list;
      this.isLoadingResults = false;
    });
  }

}
