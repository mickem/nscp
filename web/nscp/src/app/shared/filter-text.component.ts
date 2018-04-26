import { Component, EventEmitter, Output, Input } from '@angular/core';

@Component({
  selector: 'nscp-filter-text',
  template: '<mat-form-field><input matInput type="text" id="filterText" [(ngModel)]="filter" placeholder="Filter" (keyup)="filterChanged($event)" /></mat-form-field>'
})
export class FilterTextComponent {
  @Output() changed: EventEmitter<string>;

  private _filter: string;

  constructor() {
    this.changed = new EventEmitter<string>();
  }

  @Input()
  set filter(filter: string) {
    this._filter = filter || '';
    this._filter = this._filter.trim();
  }
 
  get filter(): string { return this._filter; }

  clear() {
    this.filter = '';
  }

  filterChanged(event: any) {
    event.preventDefault();
    console.log(`Filter Changed: ${this.filter}`);
    this.changed.emit(this.filter);
  }
}