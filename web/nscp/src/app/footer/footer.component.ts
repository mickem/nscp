import { Component, OnInit } from '@angular/core';
import {TranslateService} from '@ngx-translate/core';

@Component({
  selector: 'app-footer',
  templateUrl: './footer.component.html',
  styleUrls: ['./footer.component.css']
})
export class FooterComponent implements OnInit {
  currentLang: string;
  currentDate = Date.now();

  constructor(private translateService: TranslateService) {
    translateService.addLangs(['en']);
    translateService.setDefaultLang('en');
    this.currentLang = this.translateService.currentLang;
  }

  ngOnInit() {
  }

}
