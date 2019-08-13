import { ItemWithId } from '../services'

export const filterListById = <T extends ItemWithId>(modules: T[], module: T, action: { (m: T): T; }) => {
    return modules!.map(m => {
      if (m.id === module.id) {
        return action(m);
      }
      return m;
    })
  }
  